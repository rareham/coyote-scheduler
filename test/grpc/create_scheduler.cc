// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <thread>
#include "test.h"

using namespace coyote;

int main()
{
	std::cout << "[test] started." << std::endl;
	auto start_time = std::chrono::steady_clock::now();

	try
	{
		SchedulerClient* scheduler = new SchedulerClient("5f40e98d-725a-4042-9b3e-0627b1501433", "localhost:5000");
		scheduler->attach();
		//assert(scheduler->error_code(), ErrorCode::Success);
		//scheduler->detach();
		//assert(scheduler->error_code(), ErrorCode::Success);
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
