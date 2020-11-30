// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#ifndef COYOTE_OPERATION_H
#define COYOTE_OPERATION_H

#include <condition_variable>
#include <unordered_set>
#include <vector>

namespace coyote
{
	class Operation
	{
	public:
		// The unique id of this operation.
		const std::string id;

		// Conditional variable that can be used to block and schedule this operation.
		std::condition_variable cv;

		// True if this operation is currently scheduled, else false.
		bool is_scheduled;

		// True if this operation is completed, else false.
		bool is_completed;

		Operation(const std::string operation_id) noexcept :
			id(operation_id),
			is_scheduled(false),
			is_completed(false)
		{
		}

		Operation(Operation&& op) = delete;
		Operation(Operation const&) = delete;

		Operation& operator=(Operation&& op) = delete;
		Operation& operator=(Operation const&) = delete;
	};
}

#endif // COYOTE_OPERATION_H
