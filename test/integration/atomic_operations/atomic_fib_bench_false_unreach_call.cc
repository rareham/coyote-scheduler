#include <iostream>
#include <thread>

#include "test.h"
#include "coyote/atomicoperations/atomicoperations.h"

using namespace coyote;

constexpr auto WORK_THREAD_1_ID = 2;
constexpr auto WORK_THREAD_2_ID = 3;

coyote::Scheduler *scheduler;

atomic<int> i;
atomic<int> j;

bool assert_failed = false;

#define NUM 5

static void fn1()
{
  scheduler->start_operation(WORK_THREAD_1_ID);
  int k = 0;

  for (k = 0; k < NUM; k++) {
    int temp = j.load(std::memory_order_acquire);
    atomic_fetch_add_explicit(&i, temp, std::memory_order_relaxed);
  }
  scheduler->complete_operation(WORK_THREAD_1_ID);
}

static void fn2()
{
  scheduler->start_operation(WORK_THREAD_2_ID);
  int k = 0;
  for (k = 0; k < NUM; k++) {
    int temp = i.load(std::memory_order_acquire);
    atomic_fetch_add_explicit(&j, temp, std::memory_order_relaxed);
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

      for (int x = 0; x < 1000; x++)
	{
	  atomic_init(&i, 1);
	  atomic_init(&j, 1);

	  run_iteration();
	  
	  if (!(i.load(std::memory_order_relaxed) >= 144
		|| j.load(std::memory_order_relaxed) >= 144))
	    {
	      assert_failed = true;
	    }
	  
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
