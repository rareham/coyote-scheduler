#include <iostream>
#include <thread>

#include "test.h"
#include "coyote/atomicoperations/atomicoperations.h"

using namespace coyote;

constexpr auto WORK_THREAD_1_ID = 2;
constexpr auto WORK_THREAD_2_ID = 3;
constexpr auto WORK_THREAD_3_ID = 4;

coyote::Scheduler *scheduler;

bool assert_failed = false;

#define LOOP 5

atomic<int> x;

void* fn1()
{
  scheduler->start_operation(WORK_THREAD_1_ID);
  for (int i = 0; i < LOOP; i++){
    int y = 0;
    int z = 1;
    x.compare_exchange_strong(y, z, std::memory_order_relaxed);
  }
  scheduler->complete_operation(WORK_THREAD_1_ID);
  return NULL;
}

void* fn2()
{
  scheduler->start_operation(WORK_THREAD_2_ID);
  x.load(std::memory_order_relaxed);
  scheduler->complete_operation(WORK_THREAD_2_ID);
  return NULL;	
}

void* fn3()
{
  scheduler->start_operation(WORK_THREAD_3_ID);
  for (int i = 0; i < LOOP; i++){
    int y = 0;
    int z = 1;
    x.compare_exchange_strong(z, y, std::memory_order_relaxed);
  }
  scheduler->complete_operation(WORK_THREAD_3_ID);
  return NULL;	
}


void run_iteration()
{
  scheduler->attach();

  scheduler->create_operation(WORK_THREAD_1_ID);
  std::thread t1(fn1);

  scheduler->create_operation(WORK_THREAD_2_ID);
  std::thread t2(fn2);

  scheduler->create_operation(WORK_THREAD_3_ID);
  std::thread t3(fn3);

  scheduler->schedule_next();
  
  scheduler->join_operation(WORK_THREAD_1_ID);
  scheduler->join_operation(WORK_THREAD_2_ID);
  scheduler->join_operation(WORK_THREAD_3_ID);

  t1.join();
  t2.join();
  t3.join();

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
	  atomic_init(&x, 0);

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
