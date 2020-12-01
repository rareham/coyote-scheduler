// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#ifndef COYOTE_SCHEDULER_H
#define COYOTE_SCHEDULER_H

//#include <iostream>
//#include <string>

#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <unordered_set>
#include <grpcpp/grpcpp.h>
#include "settings.h"
#include "operation.h"
#include "coyote.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using coyote::Scheduler;

namespace coyote
{
	class SchedulerClient
	{
	private:
		// The unique id of the remote scheduler.
		std::string id;

		// Provides access to the remote scheduler via gRPC.
		std::unique_ptr<Scheduler::Stub> stub;

		// Configures the scheduler.
		std::unique_ptr<Settings> settings;

		// Map from unique operation ids to operations.
		std::map<const std::string, std::unique_ptr<Operation>> operation_map;

		// Map from unique resource ids to blocked operation ids.
		std::map<size_t, std::shared_ptr<std::unordered_set<size_t>>> resource_map;

		// Mutex that synchronizes access to the scheduler.
		std::unique_ptr<std::mutex> mutex;

		// Conditional variable that can be used to block scheduling a next operation until all pending
		// operations have started.
		std::condition_variable pending_operations_cv;

		// The unique id of the currently scheduled operation.
		std::string scheduled_op_id;

		// Count of newly created operations that have not started yet.
		size_t pending_start_operation_count;

	public:
		SchedulerClient(const std::string endpoint) noexcept;
		SchedulerClient(std::unique_ptr<Settings> settings) noexcept;

		// Connects with a new remote scheduler and returns its globally unique id.
		std::string connect() noexcept;

		// Connects with an existing remote scheduler with the specified id.
		std::string connect(std::string scheduler_id) noexcept;

		// Attaches to the scheduler. This should be called at the beginning of a testing iteration.
		// It creates a main operation with id '0'.
		void attach() noexcept;

		// Detaches from the scheduler. This should be called at the end of a testing iteration.
		// It completes the main operation with id '0' and releases all controlled operations. 
		void detach() noexcept;

		// Creates a new operation with the specified id.
		void create_operation(const std::string operation_id) noexcept;
		
		// Starts executing the operation with the specified id.
		void start_operation(const std::string operation_id) noexcept;

		// Waits until the operation with the specified id has completed.
		void wait_operation(const std::string operation_id) noexcept;

		//// Waits until the operations with the specified ids have completed.
		//void wait_operations(const size_t* operation_ids, size_t size, bool wait_all) noexcept;

		// Waits the resource with the specified id to become available and schedules the next operation.
		void wait_resource(const std::string resource_id) noexcept;

		//// Waits the resources with the specified ids to become available and schedules the next operation.
		//void wait_resources(const size_t* resource_ids, size_t size, bool wait_all) noexcept;

		// Signals the waiting operation that the resource with the specified id is available.
		void signal_operation(const std::string resource_id, std::string operation_id) noexcept;

		// Signals all waiting operations that the resource with the specified id is available.
		void signal_operations(const std::string resource_id) noexcept;

		// Completes executing the operation with the specified id and schedules the next operation.
		void complete_operation(const std::string operation_id) noexcept;

		// Creates a new resource with the specified id.
		void create_resource(const std::string resource_id) noexcept;

		// Deletes the resource with the specified id.
		void delete_resource(const std::string resource_id) noexcept;

		// Schedules the next operation, which can include the currently executing operation.
		// Only operations that are not blocked nor completed can be scheduled.
		void schedule_next() noexcept;

		//// Returns a controlled nondeterministic boolean value.
		//bool next_boolean() noexcept;

		//// Returns a controlled nondeterministic integer value chosen from the [0, max_value) range.
		//int next_integer(int max_value) noexcept;

		// Returns the id of the currently scheduled operation.
		const std::string scheduled_operation_id() noexcept;

		// Returns a trace that can be used to reproduce the current testing iteration.
		const std::string trace() noexcept;

	private:
		SchedulerClient(SchedulerClient&& op) = delete;
		SchedulerClient(SchedulerClient const&) = delete;

		SchedulerClient& operator=(SchedulerClient&& op) = delete;
		SchedulerClient& operator=(SchedulerClient const&) = delete;

		void create_operation_inner(const std::string operation_id);
		void start_operation_inner(const std::string operation_id, std::unique_lock<std::mutex>& lock);
		void schedule_next_inner(const std::string operation_id, std::unique_lock<std::mutex>& lock);

		// Wait for any new operations that have not started yet.
		void wait_pending_operations(std::unique_lock<std::mutex>& lock);
	};
}

#endif // COYOTE_SCHEDULER_H
