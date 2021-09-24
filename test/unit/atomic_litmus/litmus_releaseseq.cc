/*
 * This test performs some relaxes, release, acquire opeations on a single
 * atomic variable. It can give some rough idea of release sequence support but
 * probably should be improved to give better information.
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

atomic<int> x;
int var = 0;

static void a()
{
  scheduler->start_operation(WORK_THREAD_1_ID);
  var = 1;
  atomic_store_explicit(&x, 1, operation_order::release);
  atomic_store_explicit(&x, 42, operation_order::relaxed);
  scheduler->complete_operation(WORK_THREAD_1_ID);
}

static void b()
{
  scheduler->start_operation(WORK_THREAD_2_ID);
  int r = atomic_load_explicit(&x, operation_order::acquire);
  std::cout << "r = " << r;
  std::cout  << "load = " << var;
  scheduler->complete_operation(WORK_THREAD_2_ID);
}

static void c()
{
  scheduler->start_operation(WORK_THREAD_3_ID);
  atomic_store_explicit(&x, 2, operation_order::relaxed);
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
