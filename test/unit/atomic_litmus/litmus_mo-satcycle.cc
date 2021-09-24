/**
 * @file mo-satcycle.cc
 * @brief MO satisfaction cycle test
 *
 * This program has a peculiar behavior which is technically legal under the
 * current C++ memory model but which is a result of a type of satisfaction
 * cycle. We use this as justification for part of our modifications to the
 * memory model when proving our model-checker's correctness.
 */

#include <thread>
#include <iostream>

#include "test.h"
#include "coyote/atomicoperations/atomicoperations.h"

using namespace coyote;

constexpr auto WORK_THREAD_1_ID = 2;
constexpr auto WORK_THREAD_2_ID = 3;
constexpr auto WORK_THREAD_3_ID = 4;
constexpr auto WORK_THREAD_4_ID = 5;

coyote::Scheduler *scheduler;

atomic<int> x, y;
int r0, r1, r2, r3; /* "local" variables */

static void a()
{
  scheduler->start_operation(WORK_THREAD_1_ID);
  y.store(10, operation_order::relaxed);
  x.store(1, operation_order::release);
  scheduler->complete_operation(WORK_THREAD_1_ID);
}

static void b()
{
  scheduler->start_operation(WORK_THREAD_2_ID);
  r0 = x.load(operation_order::relaxed);
  r1 = x.load(operation_order::acquire);
  y.store(11, operation_order::relaxed);
  scheduler->complete_operation(WORK_THREAD_2_ID);
}

static void c()
{
  scheduler->start_operation(WORK_THREAD_3_ID);
  r2 = y.load(operation_order::relaxed);
  r3 = y.load(operation_order::relaxed);
  if (r2 == 11 && r3 == 10)
    {
      x.store(0, operation_order::relaxed);
    }
  scheduler->complete_operation(WORK_THREAD_3_ID);
}

void run_iteration()
{
  scheduler->attach();
  
  scheduler->create_operation(WORK_THREAD_1_ID);
  std::thread t1(a);

  scheduler->create_operation(WORK_THREAD_2_ID);
  std::thread t2(b);

  scheduler->create_operation(WORK_THREAD_3_ID);
  std::thread t3(c);

  scheduler->schedule_next();

  scheduler->join_operation(WORK_THREAD_1_ID);
  scheduler->join_operation(WORK_THREAD_2_ID);
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
	  
	  run_iteration();

	  /*
	   * This condition should not be hit because it only occurs under a
	   * satisfaction cycle
	   */
	  bool cycle = (r0 == 1 && r1 == 0 && r2 == 11 && r3 == 10);
	  assert(!cycle, "synchronisation failed!");
	  
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
