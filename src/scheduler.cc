// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <iostream>
#include <vector>
#include "scheduler.h"

namespace coyote
{
	SchedulerClient::SchedulerClient(const std::string endpoint) noexcept :
		SchedulerClient(endpoint, std::make_unique<Settings>())
	{
	}

	SchedulerClient::SchedulerClient(const std::string endpoint, std::unique_ptr<Settings> settings) noexcept :
		id(""),
		endpoint(endpoint),
		stub(Scheduler::NewStub(grpc::CreateChannel(
			endpoint, grpc::InsecureChannelCredentials()))),
		configuration(std::move(settings)),
		mutex(std::make_unique<std::mutex>()),
		pending_operations_cv(),
		scheduled_op_id(""),
		pending_start_operation_count(0)
	{
	}

	uint32_t SchedulerClient::init() noexcept
	{
		std::unique_lock<std::mutex> lock(*mutex);

		if (id != "")
		{
#ifdef COYOTE_DEBUG_LOG
			std::cout << "[coyote::init] remote scheduler with id '" << id << "' at " << endpoint << " is already initialized" << std::endl;
#endif // COYOTE_DEBUG_LOG
			return 1;
		}

		InitializeRequest request;
		if (configuration->exploration_strategy() == StrategyType::PCT)
		{
			request.set_strategy_type("pct");
		}
		else
		{
			request.set_strategy_type("random");
		}

		request.set_strategy_bound(configuration->exploration_strategy_bound());
		request.set_random_seed(configuration->random_seed());

		InitializeReply reply;
		ClientContext context;

		Status status = stub->Initialize(&context, request, &reply);
		if (status.ok())
		{
			id = reply.scheduler_id();
#ifdef COYOTE_DEBUG_LOG
			std::cout << "[coyote::init] initialized remote scheduler with id '" << id << "' at " << endpoint << std::endl;
#endif // COYOTE_DEBUG_LOG
			return reply.error_code();
		}
		else
		{
			std::cout << status.error_code() << ": " << status.error_message() << std::endl;
			return 1;
		}
	}

	uint32_t SchedulerClient::attach() noexcept
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::attach] attaching to remote scheduler with id '" << id << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG

		AttachRequest request;
		request.set_scheduler_id(id);

		AttachReply reply;
		ClientContext context;

		Status status = stub->Attach(&context, request, &reply);
		if (status.ok())
		{
			std::string main_op_id = reply.main_operation_id();
			create_operation_inner(main_op_id);
			start_operation_inner(main_op_id, lock);
			return reply.error_code();
		}
		else
		{
			std::cout << status.error_code() << ": " << status.error_message() << std::endl;
			return 1;
		}
	}

	uint32_t SchedulerClient::detach() noexcept
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::detach] detaching from remote scheduler with id '" << id << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG

		DetachRequest request;
		request.set_scheduler_id(id);

		DetachReply reply;
		ClientContext context;

		Status status = stub->Detach(&context, request, &reply);
		if (status.ok())
		{
			for (auto& kvp : operation_map)
			{
				Operation* next_op = kvp.second.get();
				if (!next_op->is_completed)
				{
					// If the operation has not already completed, then cancel it.
					next_op->is_scheduled = true;
					next_op->is_completed = true;
					next_op->cv.notify_all();
				}
			}

			pending_start_operation_count = 0;
			operation_map.clear();
			resource_map.clear();

			return reply.error_code();
		}
		else
		{
			std::cout << status.error_code() << ": " << status.error_message() << std::endl;
			return 1;
		}
	}

	uint32_t SchedulerClient::create_operation(const std::string operation_id) noexcept
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::create_operation] creating operation '" << operation_id << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG

		CreateOperationRequest request;
		request.set_scheduler_id(id);
		request.set_operation_id(operation_id);

		CreateOperationReply reply;
		ClientContext context;

		Status status = stub->CreateOperation(&context, request, &reply);
		if (status.ok())
		{
			create_operation_inner(operation_id);
			return reply.error_code();
		}
		else
		{
			std::cout << status.error_code() << ": " << status.error_message() << std::endl;
			return 1;
		}
	}

	uint32_t SchedulerClient::start_operation(const std::string operation_id) noexcept
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::start_operation] starting operation '" << operation_id << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG

		StartOperationRequest request;
		request.set_scheduler_id(id);
		request.set_operation_id(operation_id);

		StartOperationReply reply;
		ClientContext context;

		Status status = stub->StartOperation(&context, request, &reply);
		if (status.ok())
		{
			start_operation_inner(operation_id, lock);
			return reply.error_code();
		}
		else
		{
			std::cout << status.error_code() << ": " << status.error_message() << std::endl;
			return 1;
		}
	}

	uint32_t SchedulerClient::join_operation(const std::string operation_id) noexcept
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::join_operation] joining operation '" << operation_id << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG

		// Wait for any recently created operations to start.
		wait_pending_operations(lock);

		WaitOperationRequest request;
		request.set_scheduler_id(id);
		request.set_operation_id(operation_id);

		WaitOperationReply reply;
		ClientContext context;

		Status status = stub->WaitOperation(&context, request, &reply);
		if (status.ok())
		{
			schedule_next_inner(reply.next_operation_id(), lock);
			return reply.error_code();
		}
		else
		{
			std::cout << status.error_code() << ": " << status.error_message() << std::endl;
			return 1;
		}
	}

	uint32_t SchedulerClient::complete_operation(const std::string operation_id) noexcept
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::complete_operation] completing operation '" << operation_id << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG

		// Wait for any recently created operations to start.
		wait_pending_operations(lock);

		CompleteOperationRequest request;
		request.set_scheduler_id(id);
		request.set_operation_id(operation_id);

		CompleteOperationReply reply;
		ClientContext context;

		Status status = stub->CompleteOperation(&context, request, &reply);
		if (status.ok())
		{
			// Complete the current operation.
			auto it = operation_map.find(operation_id);
			Operation* op = it->second.get();
			op->is_completed = true;

			// Schedule the next enabled.
			schedule_next_inner(reply.next_operation_id(), lock);
			return reply.error_code();
		}
		else
		{
			std::cout << status.error_code() << ": " << status.error_message() << std::endl;
			return 1;
		}
	}

	uint32_t SchedulerClient::schedule_next() noexcept
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::schedule_next] scheduling next operation" << std::endl;
#endif // COYOTE_DEBUG_LOG

		// Wait for any recently created operations to start.
		wait_pending_operations(lock);

		ScheduleNextRequest request;
		request.set_scheduler_id(id);

		ScheduleNextReply reply;
		ClientContext context;

		Status status = stub->ScheduleNext(&context, request, &reply);
		if (status.ok())
		{
			schedule_next_inner(reply.next_operation_id(), lock);
			return reply.error_code();
		}
		else
		{
			std::cout << status.error_code() << ": " << status.error_message() << std::endl;
			return 1;
		}
	}

	void SchedulerClient::create_operation_inner(const std::string operation_id)
	{
		auto result = operation_map.insert(std::pair<std::string, std::unique_ptr<Operation>>(
			operation_id, std::make_unique<Operation>(operation_id)));
		if (operation_map.size() == 1)
		{
			// This is the first operation, so schedule it.
			scheduled_op_id = operation_id;
			result.first->second->is_scheduled = true;
#ifdef COYOTE_DEBUG_LOG
			std::cout << "[coyote::create_operation] scheduling operation '" << scheduled_op_id << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG
		}

		// Increment the count of created operations that have not yet started.
		pending_start_operation_count += 1;
	}

	void SchedulerClient::start_operation_inner(const std::string operation_id, std::unique_lock<std::mutex>& lock)
	{
		// Decrement the count of pending operations.
		pending_start_operation_count -= 1;
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::start_operation] " << pending_start_operation_count << " operations pending" << std::endl;
#endif // COYOTE_DEBUG_LOG
		if (pending_start_operation_count == 0)
		{
			// If no pending operations remain, then release schedule next.
			pending_operations_cv.notify_all();
		}

		auto it = operation_map.find(operation_id);
		Operation* op = it->second.get();

		op->cv.notify_all();
		while (!op->is_scheduled)
		{
#ifdef COYOTE_DEBUG_LOG
			std::cout << "[coyote::start_operation] pausing operation '" << operation_id << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG
			op->cv.wait(lock);
#ifdef COYOTE_DEBUG_LOG
			std::cout << "[coyote::start_operation] resuming operation '" << operation_id << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG
		}
	}

	void SchedulerClient::schedule_next_inner(const std::string operation_id, std::unique_lock<std::mutex>& lock)
	{
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::schedule_next] scheduled operation '" << operation_id << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG

		Operation* next_op = operation_map.at(operation_id).get();

		const std::string previous_id = scheduled_op_id;
		scheduled_op_id = operation_id;

		if (previous_id != operation_id)
		{
			// Resume the next operation.
			next_op->is_scheduled = true;
			next_op->cv.notify_all();

			// Pause the previous operation.
			Operation* previous_op = operation_map.at(previous_id).get();
			if (!previous_op->is_completed)
			{
				previous_op->is_scheduled = false;

				while (!previous_op->is_scheduled)
				{
#ifdef COYOTE_DEBUG_LOG
					std::cout << "[coyote::schedule_next] pausing operation '" << previous_id << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG
					// Wait until the operation gets scheduled again.
					previous_op->cv.wait(lock);
#ifdef COYOTE_DEBUG_LOG
					std::cout << "[coyote::schedule_next] resuming operation '" << previous_id << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG
				}
			}
		}
	}

	void SchedulerClient::wait_pending_operations(std::unique_lock<std::mutex>& lock)
	{
		while (pending_start_operation_count > 0)
		{
#ifdef COYOTE_DEBUG_LOG
			std::cout << "[coyote::wait_pending_operations] waiting " << pending_start_operation_count <<
				" operations to start" << std::endl;
#endif // COYOTE_DEBUG_LOG
			pending_operations_cv.wait(lock);
		}
	}
}
