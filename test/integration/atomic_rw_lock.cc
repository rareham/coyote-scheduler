#include <iostream>
#include <thread>
//#include <atomic>

#include "test.h"
#include "coyote/operations/atomicoperations.h"

#define ASSUME_LOOP 15
#define THREAD_COUNT 2

#define SEMAPHORE_ID 1

atomic<int> w ;
atomic<int> r ;
atomic<int> x ;
atomic<int> y ;
atomic<int> flagw ;
atomic<int> flagr ;

Scheduler* scheduler;

// TODO: delete commented atomic statements
int atomic_take_write_lock() {
  int ok = 0;
  scheduler->schedule_next();
  flagw.store(1, operation_order::seq_cst);
  if(flagw.load(operation_order::relaxed)) {
    scheduler->schedule_next();
    if(!flagr.load(operation_order::acquire)) {
      scheduler->schedule_next();
      for (int k = 0; k < ASSUME_LOOP; k++) {
	if (w.load(operation_order::acquire) == 0 && r.load(operation_order::acquire) == 0){
	  scheduler->schedule_next();
	  ok = 1;
	  break;
	}
      }
    }
  }
  if (ok == 0) return 0;
  w.store(1, operation_order::relaxed);
  flagw.store(0, operation_order::release);
  return 1;
}

int atomic_take_read_lock() {
  scheduler->schedule_next();
  int ok = 0;
  flagr.store(1, operation_order::seq_cst);
  if (flagr.load(operation_order::relaxed)) {
    scheduler->schedule_next();
    if (! flagw.load(operation_order::acquire)) {
      scheduler->schedule_next();
      for (int k = 0; k < ASSUME_LOOP; k++){
	if (w.load(operation_order::acquire) == 0){
	  scheduler->schedule_next();
	  ok = 1;
	  break;
	}
      }
    }
  }
  if (ok == 0) return 0;
  atomic_fetch_add_explicit(&r, 1, operation_order::acq_rel);
  scheduler->schedule_next();
  flagr.store(0, operation_order::release);
  return 1;
}

void atomic_release_read_lock() {
  atomic_fetch_sub_explicit(&r, 1, operation_order::acq_rel);
  scheduler->schedule_next();
}

static void writer(int id) {
  scheduler->start_operation(id);
  scheduler->schedule_next();
  if(atomic_take_write_lock()) {
    x.store(3, operation_order::relaxed);
    w.store(0, operation_order::relaxed);
  }
  scheduler->complete_operation(id);
}


static void reader(int id) {
  int l;
  scheduler->start_operation(id);
  scheduler->schedule_next();
  if (atomic_take_read_lock()) {
    l = x.load(operation_order::relaxed);
    scheduler->schedule_next();
    y.store(l, operation_order::relaxed);
    int temp1 = x.load(operation_order::relaxed);
    scheduler->schedule_next();
    int temp2 = y.load(operation_order::relaxed);
    scheduler->schedule_next();
    assert((temp1 == temp2) , "reader : synchronisation unaccomplished");
    atomic_release_read_lock();
  }
  scheduler->complete_operation(id);
}


void run_iteration()
{
  scheduler->attach();

  std::vector<std::thread> write_ts;
  for (int i = 0; i < THREAD_COUNT; i++)
    {
      int thread_id = i + 1;
      scheduler->create_operation(thread_id);
      write_ts.push_back(std::thread(writer, thread_id));
    }

  scheduler->create_operation(THREAD_COUNT + 1);
  std::thread read_t(reader, THREAD_COUNT + 1);
  
  scheduler->schedule_next();

  for (int i = 0; i < THREAD_COUNT; i++)
    {
      int thread_id = i + 1;
      scheduler->join_operation(thread_id);
      write_ts[i].join();
    }
  
  scheduler->join_operation(THREAD_COUNT);
  read_t.join();

  scheduler->detach();
  assert(scheduler->error_code(), ErrorCode::Success);
}



int main()
{
  std::cout << "[test] started." << std::endl;
  auto start_time = std::chrono::steady_clock::now();

  try
    {
      scheduler = new Scheduler();

      initialise_global_state(scheduler);
		
      for (int i = 0; i < 100; i++)
	{
	  atomic_init(&x, 0);
	  atomic_init(&y, 0);
	  atomic_init(&w, 0);
	  atomic_init(&r, 0);
	  atomic_init(&flagw, 0);
	  atomic_init(&flagr, 0);

#ifdef COYOTE_DEBUG_LOG
	  std::cout << "[test] iteration " << i << std::endl;
#endif // COYOTE_DEBUG_LOG
	  run_iteration();
	  reinitialise_global_state();
	}
      
      delete global_state;
      delete scheduler;
    }
  catch (std::string error)
    {
      std::cout << "[test] failed: " << error << std::endl;
      return 1;
    }

  std::cout << "[test] done in " << total_time(start_time) << "ms." << std::endl;
	return 0;
}
