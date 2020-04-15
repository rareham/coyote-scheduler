// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <cassert>
#include <iostream>
#include <map>
#include "random_strategy.h"

namespace coyote
{
	RandomStrategy::RandomStrategy(size_t seed) noexcept :
		iteration_seed(seed),
		generator(seed)
	{
	}

	size_t RandomStrategy::next_operation(Operations& operations)
	{
		assert(operations.size() != 0);
		const size_t index = generator.next() % operations.size();
		return operations[index];
	}

	bool RandomStrategy::next_boolean()
	{
		return generator.next() & 1;
	}

	int RandomStrategy::next_integer(int max_value)
	{
		return generator.next() % max_value;
	}

	size_t RandomStrategy::seed()
	{
		return iteration_seed;
	}

	void RandomStrategy::prepare_next_iteration()
	{
		iteration_seed += 1;
		generator.seed(iteration_seed);
	}
}
