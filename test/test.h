// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#ifndef COYOTE_TEST_H
#define COYOTE_TEST_H

#include <chrono>
#include <iostream>
#include "coyote/scheduler.h"

using namespace coyote;

void check(bool predicate, std::string error)
{
	if (!predicate)
	{
		throw error;
	}
}

void check(uint32_t actual, uint32_t expected)
{
	if (actual != expected)
	{
		throw "expected the '" + std::to_string(expected) + "' error code, but received '" + std::to_string(actual) + "'.";
	}
}

size_t total_time(std::chrono::steady_clock::time_point start_time)
{
	auto end_time = std::chrono::steady_clock::now();
	return (size_t)std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
}

#endif // COYOTE_TEST_H
