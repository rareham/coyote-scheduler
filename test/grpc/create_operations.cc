// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <thread>
#include "test.h"

using namespace coyote;

const std::string WORK_THREAD_1_ID = "c4f16b2e-feda-406b-9663-19fa8576e3a5";
const std::string WORK_THREAD_2_ID = "3e55149b-be76-4bae-94cc-2f320f2eccca";

SchedulerClient* scheduler;

void work_1()
{
	scheduler->start_operation(WORK_THREAD_1_ID);
	scheduler->complete_operation(WORK_THREAD_1_ID);
}

void work_2()
{
	scheduler->start_operation(WORK_THREAD_2_ID);
	scheduler->complete_operation(WORK_THREAD_2_ID);
}

void run_iteration()
{
	scheduler->attach();

	scheduler->create_operation(WORK_THREAD_1_ID);
	std::thread t1(work_1);

	scheduler->create_operation(WORK_THREAD_2_ID);
	std::thread t2(work_2);

	scheduler->join_operation(WORK_THREAD_1_ID);
	scheduler->join_operation(WORK_THREAD_2_ID);
	t1.join();
	t2.join();

	scheduler->detach();
	// assert(scheduler->error_code(), ErrorCode::Success);
}

int main()
{
	std::cout << "[test] started." << std::endl;
	auto start_time = std::chrono::steady_clock::now();

	try
	{
		scheduler = new SchedulerClient("localhost:5000");
		scheduler->init();

		for (int i = 0; i < 100; i++)
		{
#ifdef COYOTE_DEBUG_LOG
			std::cout << "[test] iteration " << i << std::endl;
#endif // COYOTE_DEBUG_LOG
			run_iteration();
		}

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
