// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#ifndef COYOTE_SETTINGS_H
#define COYOTE_SETTINGS_H

#include <chrono>
#include <stdexcept>

namespace coyote
{
	class Settings
	{
	private:
		// The execution exploration strategy.
		std::string strategy_type;

		// A strategy-specific bound.
		size_t strategy_bound;

		// The trace to replay.
		std::string reproducible_trace;

		// The seed used by randomized strategies.
		uint64_t seed_state;

	public:
		// The endpoint of the remote scheduler.
		const std::string endpoint;

		Settings(const std::string endpoint) noexcept :
			endpoint(endpoint),
			strategy_type("random"),
			strategy_bound(0),
			reproducible_trace(""),
			seed_state(std::chrono::high_resolution_clock::now().time_since_epoch().count())
		{
		}

		Settings(Settings&& strategy) = delete;
		Settings(Settings const&) = delete;

		Settings& operator=(Settings&& strategy) = delete;
		Settings& operator=(Settings const&) = delete;

		// Installs the random exploration strategy with the specified random seed.
		void use_random_strategy(uint64_t seed) noexcept
		{
			strategy_type = "random";
			seed_state = seed;
			strategy_bound = 100;
		}

		// Installs the random exploration strategy with the specified random seed and probability
		// of deviating from the currently scheduled enabled operation.
		void use_random_strategy(uint64_t seed, size_t probability)
		{
			if (probability > 100) {
				throw std::invalid_argument("received probability greater than 100");
			}

			strategy_type = "probabilistic";
			seed_state = seed;
			strategy_bound = probability;
		}

		// Installs the PCT exploration strategy with the specified random seed and priority switch bound.
		void use_pct_strategy(uint64_t seed, size_t bound, bool is_fair) noexcept
		{
			if (is_fair)
			{
				strategy_type = "fairpct";
			}
			else
			{
				strategy_type = "pct";
			}

			seed_state = seed;
			strategy_bound = bound;
		}

		// Installs the replay exploration strategy with the specified trace.
		void use_replay_strategy(const std::string trace) noexcept
		{
			strategy_type = "replay";
			reproducible_trace = trace;
		}

		// Returns the type of the installed exploration strategy.
		const std::string exploration_strategy() noexcept
		{
			return strategy_type;
		}

		// Returns an exploration strategy specific bound.
		size_t exploration_strategy_bound() noexcept
		{
			return strategy_bound;
		}

		// Returns a reproducible trace.
		const std::string trace() noexcept
		{
			return reproducible_trace;
		}

		// Returns the seed used by randomized strategies.
		uint64_t random_seed() noexcept
		{
			return seed_state;
		}
	};
}

#endif // COYOTE_SETTINGS_H
