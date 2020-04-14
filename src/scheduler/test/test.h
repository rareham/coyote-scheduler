// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#ifndef TEST_H
#define TEST_H

#include <chrono>
#include <iostream>
#include "scheduler.h"
#include "strategies/random.h"

void assert(bool predicate, std::string error)
{
	if (!predicate)
	{
		throw error;
	}
}

void assert(std::error_code actual, coyote::ErrorCode expected)
{
	std::error_code expected_error_code = expected;
	if (actual.value() != expected_error_code.value())
	{
		throw "expected the '" + expected_error_code.message() + "' error code, but received '" +
			actual.message() + "'.";
	}
}

size_t total_time(std::chrono::steady_clock::time_point start_time)
{
	auto end_time = std::chrono::steady_clock::now();
	return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
}

#endif // TEST_H
