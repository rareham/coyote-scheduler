// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#ifndef COYOTE_SCHEDULER_CLIENT_H
#define COYOTE_SCHEDULER_CLIENT_H

//#include <iostream>
//#include <string>

#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <unordered_set>
#include <grpcpp/grpcpp.h>
#include "settings.h"
#include "operations/operation.h"
#include "operations/operations.h"
#include "proto/coyote.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using coyote::Scheduler;

namespace coyote
{
	class SchedulerClient
	{
	private:
		// The id of this scheduler.
		std::string id;

		// Provides access to Coyote via gRPC.
		std::unique_ptr<Scheduler::Stub> stub;

		// Configures the program exploration.
		std::unique_ptr<Settings> configuration;

		// Map from unique operation ids to operations.
		std::map<size_t, std::unique_ptr<Operation>> operation_map;

		// Vector of enabled and disabled operation ids.
		Operations operations;

		// Map from unique resource ids to blocked operation ids.
		std::map<size_t, std::shared_ptr<std::unordered_set<size_t>>> resource_map;

		// Mutex that synchronizes access to the scheduler.
		std::unique_ptr<std::mutex> mutex;

		// Conditional variable that can be used to block scheduling a next operation until all pending
		// operations have started.
		std::condition_variable pending_operations_cv;

		// The id of the currently scheduled operation.
		size_t scheduled_op_id;

		// Count of newly created operations that have not started yet.
		size_t pending_start_operation_count;

		// The id of the main operation.
		const size_t main_op_id = 0;

		// True if an execution is attached to the scheduler, else false.
		bool is_attached;

		// The testing iteration count. It increments on each attach.
		size_t iteration_count;

	public:
		SchedulerClient(std::string scheduler_id, std::string endpoint) noexcept;
		SchedulerClient(std::string scheduler_id, std::string endpoint, std::unique_ptr<Settings> settings) noexcept;

		// Attaches to the scheduler. This should be called at the beginning of a testing iteration.
		// It creates a main operation with id '0'.
		uint32_t attach() noexcept;

		// Detaches from the scheduler. This should be called at the end of a testing iteration.
		// It completes the main operation with id '0' and releases all controlled operations. 
		uint32_t detach() noexcept;

		// Creates a new operation with the specified id.
		uint32_t create_operation(size_t operation_id) noexcept;
		
		// Starts executing the operation with the specified id.
		uint32_t start_operation(size_t operation_id) noexcept;

		// Waits until the operation with the specified id has completed.
		uint32_t join_operation(size_t operation_id) noexcept;

		//// Waits until the operations with the specified ids have completed.
		//uint32_t join_operations(const size_t* operation_ids, size_t size, bool wait_all) noexcept;

		// Completes executing the operation with the specified id and schedules the next operation.
		uint32_t complete_operation(size_t operation_id) noexcept;

		//// Creates a new resource with the specified id.
		//uint32_t create_resource(size_t resource_id) noexcept;

		//// Waits the resource with the specified id to become available and schedules the next operation.
		//uint32_t wait_resource(size_t resource_id) noexcept;
		//
		//// Waits the resources with the specified ids to become available and schedules the next operation.
		//uint32_t wait_resources(const size_t* resource_ids, size_t size, bool wait_all) noexcept;
  //      
		//// Signals all waiting operations that the resource with the specified id is available.
		//uint32_t signal_resource(size_t resource_id) noexcept;

		//// Signals the waiting operation that the resource with the specified id is available.
		//uint32_t signal_resource(size_t resource_id, size_t operation_id) noexcept;

		//// Deletes the resource with the specified id.
		//uint32_t delete_resource(size_t resource_id) noexcept;

		// Schedules the next operation, which can include the currently executing operation.
		// Only operations that are not blocked nor completed can be scheduled.
		uint32_t schedule_next() noexcept;

		//// Returns a controlled nondeterministic boolean value.
		//bool next_boolean() noexcept;

		//// Returns a controlled nondeterministic integer value chosen from the [0, max_value) range.
		//int next_integer(int max_value) noexcept;

		//// Returns the id of the currently scheduled operation.
		//size_t scheduled_operation_id() noexcept;

		//// Returns a seed that can be used to reproduce the current testing iteration.
		//uint64_t random_seed() noexcept;

	private:
		SchedulerClient(SchedulerClient&& op) = delete;
		SchedulerClient(SchedulerClient const&) = delete;

		SchedulerClient& operator=(SchedulerClient&& op) = delete;
		SchedulerClient& operator=(SchedulerClient const&) = delete;

		//void create_operation_inner(size_t operation_id);
		//void start_operation_inner(size_t operation_id, std::unique_lock<std::mutex>& lock);
		//void schedule_next_inner(std::unique_lock<std::mutex>& lock);
	};
}

#endif // COYOTE_SCHEDULER_CLIENT_H
