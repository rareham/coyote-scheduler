#include <iostream>
#include <thread>

#include "test.h"
#include "coyote/atomicoperations/atomicoperations.h"

constexpr auto WORK_THREAD_1_ID = 2;
constexpr auto WORK_THREAD_2_ID = 3;

using namespace coyote;

Scheduler* scheduler;

atomic<int> x;
atomic<int> y;
int z;

static void fn1()
{
  scheduler->start_operation(WORK_THREAD_1_ID);
  x.store(1, std::memory_order_seq_cst);
  if (y.load(std::memory_order_relaxed) == 0) {
    z = 1;
  }
  scheduler->complete_operation(WORK_THREAD_1_ID);
}

static void fn2()
{
  scheduler->start_operation(WORK_THREAD_2_ID);
  y.store(1, std::memory_order_seq_cst);
  if (x.load(std::memory_order_relaxed) == 0) {
    int k = z;
  }
  scheduler->complete_operation(WORK_THREAD_2_ID);
}


void run_iteration()
{
  scheduler->attach();

  scheduler->create_operation(WORK_THREAD_1_ID);
  std::thread t1(fn1);
  
  scheduler->create_operation(WORK_THREAD_2_ID);
  std::thread t2(fn2);

  scheduler->schedule_next();

  scheduler->join_operation(WORK_THREAD_1_ID);
  scheduler->join_operation(WORK_THREAD_2_ID);

  t1.join();
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
      
      for (int i = 0; i < 1000; i++)
	{
	  atomic_init(&x, 0);
	  atomic_init(&y, 0);
	  z = 0;

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
