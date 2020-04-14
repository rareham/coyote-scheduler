// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#ifndef RANDOM_H
#define RANDOM_H

#include <random>
#include "operations/operation.h"

namespace coyote
{
	// Implements the xeroshiro p64r32 pseudorandom number generator.
	class Random
	{
	private:
		static constexpr unsigned BITS = 8 * sizeof(size_t);

		size_t state_x;
		size_t state_y;

	public:
		Random(size_t seed) noexcept :
			state_x(seed),
			state_y(seed == 0 ? 5489 : 0)
		{
			next();
		}

		Random(Random&& strategy) = delete;
		Random(Random const&) = delete;

		Random& operator=(Random&& strategy) = delete;
		Random& operator=(Random const&) = delete;

		void seed(const size_t seed)
		{
			state_x = seed;
			state_y = seed == 0 ? 5489 : 0;
			next();
		}

		// Returns the next random number.
		size_t next()
		{
			const uint64_t x = state_x;
			uint64_t y = state_y;
			const uint64_t result = state_x + y;

			y ^= x;
			state_x = rotl(x, 24) ^ y ^ (y << 16);
			state_y = rotl(y, 37);

			return result;
		}

	private:
		static inline size_t rotl(const size_t x, const size_t k)
		{
			return (x << k) | (x >> (BITS - k));
		}
	};
}

#endif // RANDOM_H
