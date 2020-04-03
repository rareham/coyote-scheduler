// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#ifndef OPERATION_H
#define OPERATION_H

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <unordered_set>
#include "operation_status.h"

namespace coyote
{
	class Operation
	{
	private:
		// Set of ids of each resource that the operation is waiting to be released.
		std::unordered_set<size_t> pending_resource_ids;

	public:
		// The unique id of the operation.
		const size_t id;

		// The current status of the operation.
		OperationStatus status;

		// Conditional variable that can be used to block and schedule the operation.
		std::condition_variable cv;

		// True if the operation is currently scheduled, else false.
		bool is_scheduled;

		Operation(size_t operation_id) noexcept;
		Operation(Operation&& op) = delete;
		Operation(Operation const&) = delete;

		Operation& operator=(Operation&& op) = delete;
		Operation& operator=(Operation const&) = delete;

		// Waits until the specified resource sends a signal.
		void wait_resource_signal(size_t resource_id);

		// Waits until the specified resources send a signal.
		void wait_resource_signals(const size_t* resource_ids, int array_size, bool wait_all);

		// Invoked when the specified resource sends a signal.
		void on_resource_signal(size_t resource_id);
	};
}

#endif // OPERATION_H
