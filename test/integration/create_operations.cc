// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <thread>
#include "test.h"

using namespace coyote;

const std::string WORK_THREAD_1_ID = "00000000-0000-0000-0000-000000000001";
const std::string WORK_THREAD_2_ID = "00000000-0000-0000-0000-000000000002";

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

	scheduler->wait_operation(WORK_THREAD_1_ID);
	scheduler->wait_operation(WORK_THREAD_2_ID);
	t1.join();
	t2.join();

	scheduler->detach();
}

int main()
{
	std::cout << "[test] started." << std::endl;
	auto start_time = std::chrono::steady_clock::now();

	try
	{
		scheduler = new SchedulerClient("localhost:5000");
		scheduler->connect();

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
