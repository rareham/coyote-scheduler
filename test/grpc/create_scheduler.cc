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
		SchedulerClient* scheduler = new SchedulerClient("localhost:5000");
		auto error_code = scheduler->init();
		check(error_code, 0);
		error_code = scheduler->attach();
		check(error_code, 0);
		error_code = scheduler->detach();
		check(error_code, 0);
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
