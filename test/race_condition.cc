// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <thread>
#include "test.h"

using namespace coyote;

const std::string WORK_THREAD_1_ID = "00000000-0000-0000-0000-000000000001";
const std::string WORK_THREAD_2_ID = "00000000-0000-0000-0000-000000000002";

SchedulerClient* scheduler;

int shared_var;
bool race_found;
std::string trace;

void work_1()
{
	scheduler->start_operation(WORK_THREAD_1_ID);

	shared_var = 1;
	scheduler->schedule_next();
	if (shared_var != 1)
	{
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[test] found race condition in thread 1." << std::endl;
#endif // COYOTE_DEBUG_LOG
		race_found = true;
	}

	scheduler->complete_operation(WORK_THREAD_1_ID);
}

void work_2()
{
	scheduler->start_operation(WORK_THREAD_2_ID);

	shared_var = 2;
	scheduler->schedule_next();
	if (shared_var != 2)
	{
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[test] found race condition in thread 2." << std::endl;
#endif // COYOTE_DEBUG_LOG
		race_found = true;
	}

	scheduler->complete_operation(WORK_THREAD_2_ID);
}

void run_iteration()
{
	scheduler->attach();

	scheduler->create_operation(WORK_THREAD_1_ID);
	std::thread t1(work_1);

	scheduler->create_operation(WORK_THREAD_2_ID);
	std::thread t2(work_2);

	scheduler->schedule_next();

	scheduler->wait_operation(WORK_THREAD_1_ID);
	scheduler->wait_operation(WORK_THREAD_2_ID);
	t1.join();
	t2.join();

	scheduler->detach();
}

void test()
{
	scheduler = new SchedulerClient("localhost:5000");
	scheduler->connect();

	for (int i = 0; i < 100; i++)
	{
		// Initialize the state for the test iteration.
		shared_var = 0;
		race_found = false;

#ifdef COYOTE_DEBUG_LOG
		std::cout << "[test] iteration " << i << std::endl;
#endif // COYOTE_DEBUG_LOG
		run_iteration();
		if (race_found)
		{
			trace = scheduler->trace();
			break;
		}
	}

	assert(race_found, "race was not found.");
	delete scheduler;
}

void replay()
{
	auto settings = std::make_unique<Settings>("localhost:5000");
	settings->use_replay_strategy(trace);

	scheduler = new SchedulerClient(std::move(settings));
	scheduler->connect();

	// Initialize the state for replaying.
	shared_var = 0;
	race_found = false;

	std::cout << "[test] replaying using given trace" << std::endl;
	run_iteration();

	assert(race_found, "race was not found.");
	delete scheduler;
}

int main()
{
	std::cout << "[test] started." << std::endl;
	auto start_time = std::chrono::steady_clock::now();

	try
	{
		// Try to find the race condition.
		test();

		// Try to replay the bug.
		replay();
	}
	catch (std::string error)
	{
		std::cout << "[test] failed: " << error << std::endl;
		return 1;
	}

	std::cout << "[test] done in " << total_time(start_time) << "ms." << std::endl;
	return 0;
}
