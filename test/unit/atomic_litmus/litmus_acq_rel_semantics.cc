/*
 * This test performs some relaxes, release, acquire opeations on a single
 * atomic variable. It is designed for creating a difficult set of pending
 * release sequences to resolve at the end of an execution. However, it
 * utilizes 6 threads, so it blows up into a lot of executions quickly.
 */

#include <iostream>
#include <thread>

#include "test.h"
#include "coyote/atomicoperations/atomicoperations.h"

using namespace coyote;

constexpr auto WORK_THREAD_1_ID = 2;
constexpr auto WORK_THREAD_2_ID = 3;
constexpr auto WORK_THREAD_3_ID = 4;
constexpr auto WORK_THREAD_4_ID = 5;

coyote::Scheduler *scheduler;

atomic<int> x;
int var = 0;

static void a(int i)
{
  scheduler->start_operation(WORK_THREAD_1_ID);
  atomic_store_explicit(&x, i, operation_order::seq_cst);
  atomic_store_explicit(&x, i + 1, operation_order::relaxed);
  scheduler->complete_operation(WORK_THREAD_1_ID);
}

static void b()
{
  scheduler->start_operation(WORK_THREAD_2_ID);
  int i = 7;
  scheduler->schedule_next();
  int r = atomic_load_explicit(&x, operation_order::acquire);
  std::cout << "r = " << r << std::endl;
  scheduler->complete_operation(WORK_THREAD_2_ID);
}

void run_iteration()
{
  scheduler->attach();

  int i = 4;

  scheduler->create_operation(WORK_THREAD_1_ID);
  std::thread t1 (&a, i);
  scheduler->create_operation(WORK_THREAD_2_ID);
  std::thread t2 (&b);

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

int user_main(int argc, char **argv)
{

	return 0;
}
