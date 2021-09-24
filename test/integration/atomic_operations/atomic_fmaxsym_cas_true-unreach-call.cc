#include <iostream>
#include <thread>

#include "test.h"
#include "coyote/atomicoperations/atomicoperations.h"

using namespace coyote;

#define WORKPERTHREAD 5
#define THREADSMAX 2
#define LOOP 1

atomic<int> max_num;

bool assert_failed = false;

Scheduler *scheduler;

int storage[WORKPERTHREAD*THREADSMAX] = {999, 999, 999, 999, 999, 999, 999, 999, 999, 999};

static void fn1(int arg, int id) {
  scheduler->start_operation(id);
  int offset= arg;
  int i;
  int c; 
  
  for(i = offset; i < offset+WORKPERTHREAD; i++) {
    int ok = 0;
    for (int j = 0; j < LOOP; j++) {
      c = max_num.load(std::memory_order_relaxed);
      if(storage[i] > c){
	if (max_num.compare_exchange_strong(c, storage[i], std::memory_order_relaxed)) {
	  ok = 1;
	  break;
	}
      }else{
	ok = 1;
	break;
      }
    }
    if (ok == 0) continue;
    if(!(storage[i] <= max_num.load(std::memory_order_relaxed)))
      {
	assert_failed = true;
      }
  }
  scheduler->complete_operation(id);
}

void run_iteration()
{
  scheduler->attach();

  std::thread t[THREADSMAX];
  int offsets[THREADSMAX];
  int offset = 0;

  for (int i = 0; i < THREADSMAX; i++) {
    offsets[i] = offset;
    offset += WORKPERTHREAD;
  }

  for (int i = 0; i < THREADSMAX; i++) {
    scheduler->start_operation(i);
    t[i] = std::thread(fn1, offsets[i], i);
  }

  scheduler->schedule_next();
  
  for (int i = 0; i < THREADSMAX; i++) {
    scheduler->join_operation(i);
    t[i].join();
  }
  
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

      for (int i = 0; i < 1000; i++)
	{
	  atomic_init(&max_num, 0);
	  run_iteration();

	  ASSERT_PASS_CHECK
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
