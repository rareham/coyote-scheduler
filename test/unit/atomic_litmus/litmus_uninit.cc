/**
 * @file uninit.cc
 * @brief Uninitialized loads test
 *
 * This is a test of the "uninitialized loads" code. While we don't explicitly
 * initialize y, this example's synchronization pattern should guarantee we
 * never see it uninitialized.
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
atomic<int> y;

static void a()
{
  scheduler->start_operation(WORK_THREAD_1_ID);
  int flag = x.load(operation_order::acquire);
  std::cout << "flag : " << flag << std::endl;
  if (flag == 2)
    {
      int tempy = y.load(operation_order::relaxed);
      std::cout << "tempy : " << tempy << std::endl;
    }
  scheduler->complete_operation(WORK_THREAD_1_ID);
}

static void b()
{
  scheduler->start_operation(WORK_THREAD_2_ID);
  int y = x.fetch_add(1, operation_order::relaxed);
  std::cout << "y : " << y << std::endl;
  scheduler->complete_operation(WORK_THREAD_2_ID);
}

static void c()
{
  scheduler->start_operation(WORK_THREAD_3_ID);
  y.store(3, operation_order::relaxed);
  x.store(1, operation_order::release);
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
