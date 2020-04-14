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

	size_t RandomStrategy::next_operation(const std::vector<std::shared_ptr<Operation>>& operations)
	{
		assert(!operations.empty());
		const size_t index = generator.next() % operations.size();
		return operations[index]->id;
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
