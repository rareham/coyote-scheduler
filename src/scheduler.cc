// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <iostream>
#include <vector>
#include "scheduler.h"

namespace coyote
{
	SchedulerClient::SchedulerClient(const std::string endpoint) noexcept :
		SchedulerClient(std::make_unique<Settings>(endpoint))
	{
	}

	SchedulerClient::SchedulerClient(std::unique_ptr<Settings> settings) noexcept :
		id(""),
		settings(std::move(settings)),
		mutex(std::make_unique<std::mutex>()),
		pending_operations_cv(),
		scheduled_op_id(""),
		pending_start_operation_count(0)
	{
	}

	std::string SchedulerClient::connect() noexcept
	{
		return connect("");
	}

	std::string SchedulerClient::connect(std::string scheduler_id) noexcept
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::connect] connecting to remote scheduler at " << settings->endpoint << std::endl;
#endif // COYOTE_DEBUG_LOG

		stub = Scheduler::NewStub(grpc::CreateChannel(
			settings->endpoint, grpc::InsecureChannelCredentials()));

		InitializeRequest request;
		request.set_scheduler_id(scheduler_id);
		request.set_strategy_type(settings->exploration_strategy());
		request.set_strategy_bound(settings->exploration_strategy_bound());
		request.set_trace(settings->trace());
		request.set_random_seed(settings->random_seed());

		InitializeReply reply;
		ClientContext context;

		Status status = stub->Initialize(&context, request, &reply);
		if (!status.ok())
		{
			throw std::runtime_error(status.error_code() + ": " + status.error_message());
		}

		id = reply.scheduler_id();
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::connect] connected to remote scheduler with id '" << id << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG
		return id;
	}

	void SchedulerClient::attach() noexcept
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
		if (!status.ok())
		{
			throw std::runtime_error(status.error_code() + ": " + status.error_message());
		}

		std::string main_op_id = reply.main_operation_id();
		create_operation_inner(main_op_id);
		start_operation_inner(main_op_id, lock);
	}

	void SchedulerClient::detach() noexcept
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
		if (!status.ok())
		{
			throw std::runtime_error(status.error_code() + ": " + status.error_message());
		}

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
	}

	void SchedulerClient::create_operation(const std::string operation_id) noexcept
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
		if (!status.ok())
		{
			throw std::runtime_error(status.error_code() + ": " + status.error_message());
		}

		create_operation_inner(operation_id);
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

	void SchedulerClient::start_operation(const std::string operation_id) noexcept
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
		if (!status.ok())
		{
			throw std::runtime_error(status.error_code() + ": " + status.error_message());
		}

		start_operation_inner(operation_id, lock);
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

	void SchedulerClient::wait_operation(const std::string operation_id) noexcept
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::wait_operation] joining operation '" << operation_id << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG

		// Wait for any recently created operations to start.
		wait_pending_operations(lock);

		WaitOperationRequest request;
		request.set_scheduler_id(id);
		request.set_operation_id(operation_id);

		WaitOperationReply reply;
		ClientContext context;

		Status status = stub->WaitOperation(&context, request, &reply);
		if (!status.ok())
		{
			throw std::runtime_error(status.error_code() + ": " + status.error_message());
		}

		schedule_next_inner(reply.next_operation_id(), lock);
	}

	void SchedulerClient::wait_resource(const std::string resource_id) noexcept
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::wait_resource] waiting resource '" << resource_id << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG

		WaitResourceRequest request;
		request.set_scheduler_id(id);
		request.set_resource_id(resource_id);

		WaitResourceReply reply;
		ClientContext context;

		Status status = stub->WaitResource(&context, request, &reply);
		if (!status.ok())
		{
			throw std::runtime_error(status.error_code() + ": " + status.error_message());
		}

		schedule_next_inner(reply.next_operation_id(), lock);
	}

	void SchedulerClient::signal_operation(const std::string resource_id, std::string operation_id) noexcept
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::signal_operation] signaling operation '" << operation_id << "' waiting resource '"
			<< resource_id << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG

		SignalOperationRequest request;
		request.set_scheduler_id(id);
		request.set_resource_id(resource_id);
		request.set_operation_id(operation_id);

		SignalOperationReply reply;
		ClientContext context;

		Status status = stub->SignalOperation(&context, request, &reply);
		if (!status.ok())
		{
			throw std::runtime_error(status.error_code() + ": " + status.error_message());
		}
	}

	void SchedulerClient::signal_operations(const std::string resource_id) noexcept
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::signal_operations] signaling all operations waiting resource '" << resource_id << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG

		SignalOperationsRequest request;
		request.set_scheduler_id(id);
		request.set_resource_id(resource_id);

		SignalOperationsReply reply;
		ClientContext context;

		Status status = stub->SignalOperations(&context, request, &reply);
		if (!status.ok())
		{
			throw std::runtime_error(status.error_code() + ": " + status.error_message());
		}
	}

	void SchedulerClient::complete_operation(const std::string operation_id) noexcept
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
		if (!status.ok())
		{
			throw std::runtime_error(status.error_code() + ": " + status.error_message());
		}

		// Complete the current operation.
		auto it = operation_map.find(operation_id);
		Operation* op = it->second.get();
		op->is_completed = true;

		// Schedule the next enabled.
		schedule_next_inner(reply.next_operation_id(), lock);
	}

	void SchedulerClient::create_resource(const std::string resource_id) noexcept
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::create_resource] creating resource '" << resource_id << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG

		CreateResourceRequest request;
		request.set_scheduler_id(id);
		request.set_resource_id(resource_id);

		CreateResourceReply reply;
		ClientContext context;

		Status status = stub->CreateResource(&context, request, &reply);
		if (!status.ok())
		{
			throw std::runtime_error(status.error_code() + ": " + status.error_message());
		}
	}

	void SchedulerClient::delete_resource(const std::string resource_id) noexcept
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::delete_resource] deleting resource '" << resource_id << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG

		DeleteResourceRequest request;
		request.set_scheduler_id(id);
		request.set_resource_id(resource_id);

		DeleteResourceReply reply;
		ClientContext context;

		Status status = stub->DeleteResource(&context, request, &reply);
		if (!status.ok())
		{
			throw std::runtime_error(status.error_code() + ": " + status.error_message());
		}
	}

	void SchedulerClient::schedule_next() noexcept
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
		if (!status.ok())
		{
			throw std::runtime_error(status.error_code() + ": " + status.error_message());
		}

		schedule_next_inner(reply.next_operation_id(), lock);
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

	bool SchedulerClient::next_boolean() noexcept
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::schedule_next] choosing next boolean" << std::endl;
#endif // COYOTE_DEBUG_LOG

		GetNextBooleanRequest request;
		request.set_scheduler_id(id);

		GetNextBooleanReply reply;
		ClientContext context;

		Status status = stub->GetNextBoolean(&context, request, &reply);
		if (!status.ok())
		{
			throw std::runtime_error(status.error_code() + ": " + status.error_message());
		}

		bool value = reply.value();
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::schedule_next] chose '" << value << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG
		return value;
	}

	int SchedulerClient::next_integer(int max_value) noexcept
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::schedule_next] choosing next integer" << std::endl;
#endif // COYOTE_DEBUG_LOG

		GetNextIntegerRequest request;
		request.set_scheduler_id(id);
		request.set_max_value(max_value);

		GetNextIntegerReply reply;
		ClientContext context;

		Status status = stub->GetNextInteger(&context, request, &reply);
		if (!status.ok())
		{
			throw std::runtime_error(status.error_code() + ": " + status.error_message());
		}

		int value = reply.value();
#ifdef COYOTE_DEBUG_LOG
		std::cout << "[coyote::schedule_next] chose '" << value << "'" << std::endl;
#endif // COYOTE_DEBUG_LOG
		return value;
	}

	void SchedulerClient::wait_pending_operations(std::unique_lock<std::mutex>& lock)
	{
		while (pending_start_operation_count > 0)
		{
#ifdef COYOTE_DEBUG_LOG
			std::cout << "[coyote::wait_pending_operations] waiting " << pending_start_operation_count
				<< " operations to start" << std::endl;
#endif // COYOTE_DEBUG_LOG
			pending_operations_cv.wait(lock);
		}
	}

	const std::string SchedulerClient::scheduled_operation_id() noexcept
	{
		return scheduled_op_id;
	}

	const std::string SchedulerClient::trace() noexcept
	{
		std::unique_lock<std::mutex> lock(*mutex);

		GetTraceRequest request;
		request.set_scheduler_id(id);

		GetTraceReply reply;
		ClientContext context;

		Status status = stub->GetTrace(&context, request, &reply);
		if (!status.ok())
		{
			throw std::runtime_error(status.error_code() + ": " + status.error_message());
		}

		return reply.trace();
	}
}
