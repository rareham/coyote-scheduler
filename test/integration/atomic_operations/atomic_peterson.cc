#include <iostream>
#include <thread>

#include "test.h"
#include "coyote/atomicoperations/atomicoperations.h"

using namespace coyote;

constexpr auto WORK_THREAD_1_ID = 2;
constexpr auto WORK_THREAD_2_ID = 3;

coyote::Scheduler *scheduler;

bool assert_failed = false;

atomic<int> flag1;
atomic<int> flag2;
atomic<int> turn;
atomic<int> x;

#define LOOP 5

static void thr1() {
  scheduler->start_operation(WORK_THREAD_1_ID);
    int ok1 = 0;
    
    flag1.store(1, std::memory_order_seq_cst);//a
    turn.store(2, std::memory_order_seq_cst);//b

    for (int k = 0; k < LOOP; k++) {
        if (not (flag2.load(std::memory_order_acquire) == 1 &&
                turn.load(std::memory_order_acquire) == 2)) {
            ok1 = 1;
            break; 
        }
    }
    if (ok1 == 0)
      {
	scheduler->complete_operation(WORK_THREAD_1_ID);
	return;
      }

    // begin: critical section
    x.store(1, std::memory_order_relaxed);
    int tempx = x.load(std::memory_order_relaxed);
    
    if(!(tempx == 1))
      {
	assert_failed = true;
      }
    // end: critical section

    flag1.store(0, std::memory_order_seq_cst);
    scheduler->complete_operation(WORK_THREAD_1_ID);
}


static void thr2() {
  scheduler->start_operation(WORK_THREAD_2_ID);
  int ok1 = 0;
  
  flag2.store(1, std::memory_order_seq_cst);//c 
  turn.store(1, std::memory_order_seq_cst);//d
  
  for (int k = 0; k < LOOP; k++) {
    if (not (flag1.load(std::memory_order_acquire) == 1 &&
	     turn.load(std::memory_order_acquire) == 1)) {
      ok1 = 1;
      break;
    }
  }
  if (ok1 == 0)
    {
      scheduler->complete_operation(WORK_THREAD_2_ID);
      return;
    }
  
  // begin: critical section
  x.store(2, std::memory_order_relaxed);

  int tempx = x.load(std::memory_order_relaxed);
  

  if(!(tempx == 2))
    {
      assert_failed = true;
    }
  // end: critical section
  
  flag2.store(0, std::memory_order_seq_cst);
  scheduler->complete_operation(WORK_THREAD_2_ID);
}


void run_iteration()
{
  scheduler->attach();

  scheduler->create_operation(WORK_THREAD_1_ID);
  std::thread t1(thr1);

  scheduler->create_operation(WORK_THREAD_2_ID);
  std::thread t2(thr2);

  scheduler->schedule_next();

  scheduler->join_operation(WORK_THREAD_1_ID);
  t1.join();
  scheduler->join_operation(WORK_THREAD_2_ID);
  t2.join();

  scheduler->detach();
  assert(scheduler->error_code(), ErrorCode::Success);
}


int main(int argc, char **argv)
{
  std::cout << "[test] started." << std::endl;
  auto start_time = std::chrono::steady_clock::now();
	
  try
    {
      scheduler = new Scheduler();
      
      initialise_global_state(scheduler);
      
      for (int i = 0; i < 100; i++)
	{
	  atomic_init(&flag1, 0);
	  atomic_init(&flag2, 0);
	  atomic_init(&turn, 0);
	  atomic_init(&x, 0);
	  
	  run_iteration();

	  ASSERT_CHECK
	  
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
