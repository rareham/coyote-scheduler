// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

//#include <atomic>
#include <iostream>
#include <iterator>
#include <thread>

#include "test.h"
#include "coyote/operations/atomicoperations.h"


using namespace coyote;

constexpr auto MAIN_THREAD_ID = 1;
constexpr auto WORK_THREAD_1_ID = 2;
constexpr auto WORK_THREAD_2_ID = 3;
constexpr auto LOCK_ID = 1;

#ifdef COYOTE_DEBUG_LOG
#undef COYOTE_DEBUG_LOG
#endif


atomic<int> x(0);
atomic<int> y(0);
Scheduler* scheduler;

void work_1()
{
	scheduler->start_operation(WORK_THREAD_1_ID);
	atomic_store(&x, 1, operation_order::relaxed);
	atomic_store(&y, 2, operation_order::relaxed);
	scheduler->schedule_next();
	scheduler->complete_operation(WORK_THREAD_1_ID);
}

void work_2()
{
	scheduler->start_operation(WORK_THREAD_2_ID);
	value loadx = atomic_load(&x, operation_order::relaxed);
 	std::cout << "work_2 :: loadx : " << loadx << std::endl;
	scheduler->schedule_next();
	value loady = atomic_load(&y, operation_order::relaxed);
	std::cout << "work_2 :: loady : " << loady << std::endl;
	scheduler->schedule_next();
	scheduler->complete_operation(WORK_THREAD_2_ID);
}

void run_iteration()
{
  scheduler->attach();

  scheduler->create_resource(LOCK_ID);
  scheduler->create_operation(WORK_THREAD_1_ID);
  std::thread t1(work_1);

  scheduler->create_operation(WORK_THREAD_2_ID);
  std::thread t2(work_2);
  scheduler->join_operation(WORK_THREAD_1_ID);
  scheduler->join_operation(WORK_THREAD_2_ID);
  t1.join();
  t2.join();

  scheduler->detach();
  assert(scheduler->error_code(), ErrorCode::Success);
}

int main()
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

#ifdef COYOTE_DEBUG_LOG
	  std::cout << "[test] iteration " << i << std::endl;
#endif // COYOTE_DEBUG_LOG
	  run_iteration();
	  reinitialise_global_state();
	}
      // TODO: dont manage global_state here
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
