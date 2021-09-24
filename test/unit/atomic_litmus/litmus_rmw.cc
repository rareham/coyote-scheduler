#include <thread>
#include <iostream>

#include "test.h"
#include "coyote/atomicoperations/atomicoperations.h"

using namespace coyote;

constexpr auto WORK_THREAD_1_ID = 2;
constexpr auto WORK_THREAD_2_ID = 3;

coyote::Scheduler *scheduler;

atomic<int> x;
atomic<int> y;

static void a()
{
  scheduler->start_operation(WORK_THREAD_1_ID);
  int v1=atomic_fetch_add_explicit(&x, 1, operation_order::relaxed);
  int v2=atomic_fetch_add_explicit(&y, 1, operation_order::relaxed);
  std::cout << "v1 : " << v1 << "v2 : " << v2 << std::endl;
  scheduler->complete_operation(WORK_THREAD_1_ID);
}

static void b()
{
  scheduler->start_operation(WORK_THREAD_2_ID);
  int v3=atomic_fetch_add_explicit(&y, 1, operation_order::relaxed);
  int v4=atomic_fetch_add_explicit(&x, 1, operation_order::relaxed);
  std::cout << "v3 : " << v3 << "v4 : " << v4 << std::endl;
  scheduler->complete_operation(WORK_THREAD_2_ID);
}

void run_iteration()
{
  scheduler->attach();
  
  scheduler->create_operation(WORK_THREAD_1_ID);
  std::thread t1(a);

  scheduler->create_operation(WORK_THREAD_2_ID);
  std::thread t2(b);

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
