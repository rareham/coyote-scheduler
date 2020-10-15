// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#ifndef COYOTE_CONFIGURATION_H
#define COYOTE_CONFIGURATION_H

#include <functional>
#include "strategies/strategy.h"

namespace coyote
{
	class Settings
	{
	private:
		// The execution exploration strategy.
		Strategy strategy;

		// The seed used by randomized strategies.
		size_t seed_state;

	public:
		Settings() noexcept;

		Settings(Settings&& strategy) = delete;
		Settings(Settings const&) = delete;

		Settings& operator=(Settings&& strategy) = delete;
		Settings& operator=(Settings const&) = delete;

		// Installs the random exploration strategy that uses the specified random seed.
		void use_random_strategy(size_t seed) noexcept;

		// Installs a randomized sleep injection exploration strategy.
		void use_sleep_injection_strategy() noexcept;

		// Disables controlled scheduling.
		void disable_scheduling() noexcept;

		// Returns the type of the installed exploration strategy.
		Strategy exploration_strategy() noexcept;

		// Returns the seed used by randomized strategies.
		size_t random_seed() noexcept;
	};
}

#endif // COYOTE_CONFIGURATION_H
