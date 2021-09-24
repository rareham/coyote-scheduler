/*
 * This test performs some relaxes, release, acquire opeations on a single
 * atomic variable. It is designed for creating a difficult set of pending
 * release sequences to resolve at the end of an execution. However, it
 * utilizes 6 threads, so it blows up into a lot of executions quickly.
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
constexpr auto WORK_THREAD_5_ID = 6;

coyote::Scheduler *scheduler;

atomic<int> x;
int var = 0;

static void a(int i)
{
    scheduler->start_operation(WORK_THREAD_1_ID);
    var = 1;
    atomic_store_explicit(&x, i, operation_order::release);
    atomic_store_explicit(&x, i + 1, operation_order::relaxed);
    scheduler->complete_operation(WORK_THREAD_1_ID);
}

static void a1(int i)
{
    scheduler->start_operation(WORK_THREAD_4_ID);
    var = 1;
    atomic_store_explicit(&x, i, operation_order::release);
    atomic_store_explicit(&x, i + 1, operation_order::relaxed);
    scheduler->complete_operation(WORK_THREAD_4_ID);
}

static void b2()
{
  scheduler->start_operation(WORK_THREAD_5_ID);
  int r = atomic_load_explicit(&x, operation_order::acquire);
  std::cout << "r :" << r << std::endl;
  var = 3;
  scheduler->complete_operation(WORK_THREAD_5_ID);
}

static void b1()
{
  scheduler->start_operation(WORK_THREAD_2_ID);
  int i = 7;
  int r = atomic_load_explicit(&x, operation_order::acquire);
  std::cout << "r = " << r << std::endl;
	
  var = 2;

  scheduler->create_operation(WORK_THREAD_4_ID);
  std::thread t4(a1, i);

  scheduler->create_operation(WORK_THREAD_5_ID);
  std::thread t5(b2);

  scheduler->schedule_next();
  
  scheduler->join_operation(WORK_THREAD_4_ID);
  scheduler->join_operation(WORK_THREAD_5_ID);

  t4.join();
  t5.join();
  scheduler->complete_operation(WORK_THREAD_2_ID);
}

static void c()
{
  scheduler->start_operation(WORK_THREAD_3_ID);
  atomic_store_explicit(&x, 22, operation_order::relaxed);
  scheduler->complete_operation(WORK_THREAD_3_ID);
}

void run_iteration()
{
  scheduler->attach();
  int i = 4;
  
  scheduler->create_operation(WORK_THREAD_1_ID);
  std::thread t1(a, i);

  scheduler->create_operation(WORK_THREAD_2_ID);
  std::thread t2(b1);

  scheduler->create_operation(WORK_THREAD_3_ID);
  std::thread t3(c);

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
