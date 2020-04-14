// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
#include "scheduler.h"

namespace coyote
{
	Scheduler::Scheduler() noexcept :
		Scheduler(std::chrono::high_resolution_clock::now().time_since_epoch().count())
	{
	}

	Scheduler::Scheduler(size_t seed) noexcept :
		strategy(std::make_unique<RandomStrategy>(seed)),
		mutex(std::make_unique<std::mutex>()),
		pending_operations_cv(),
		scheduled_operation_id(0),
		pending_start_operation_count(0),
		is_attached(false),
		iteration_count(0),
		last_error_code(ErrorCode::Success)
	{
	}

	std::error_code Scheduler::attach() noexcept
	{
		try
		{
			std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_LOG
			std::cout << "[coyote::attach] attaching the current operation" << std::endl;
#endif // COYOTE_LOG

			if (is_attached)
			{
				throw ErrorCode::ClientAttached;
			}

			is_attached = true;
			iteration_count += 1;
			last_error_code = ErrorCode::Success;

			if (iteration_count > 1)
			{
				// Prepare the strategy for the next iteration.
				strategy->prepare_next_iteration();
			}

			create_operation(main_operation_id, lock);
			start_operation(main_operation_id, lock);
		}
		catch (ErrorCode error_code)
		{
			last_error_code = error_code;
		}
		catch (...)
		{
			last_error_code = ErrorCode::Failure;
		}

		return last_error_code;
	}

	std::error_code Scheduler::detach() noexcept
	{
		try
		{
			std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_LOG
			std::cout << "[coyote::detach] releasing all operations" << std::endl;
#endif // COYOTE_LOG

			if (!is_attached)
			{
				throw ErrorCode::ClientNotAttached;
			}

			is_attached = false;

			std::shared_ptr<Operation> op(operation_map.at(main_operation_id));
			op->status = OperationStatus::Completed;

			for (auto& kvp : operation_map)
			{
				Operation* op = kvp.second.get();
				if (op->status != OperationStatus::Completed)
				{
#ifdef COYOTE_LOG
					std::cout << "[coyote::detach] canceling operation " << op->id << std::endl;
#endif // COYOTE_LOG
					// If the operation has not already completed, then cancel it.
					op->is_scheduled = true;
					op->status = OperationStatus::Completed;
					op->cv.notify_all();
				}
			}

			operation_map.clear();
			enabled_operation_ids.clear();
			resource_map.clear();
			pending_start_operation_count = 0;
		}
		catch (ErrorCode error_code)
		{
			last_error_code = error_code;
		}
		catch (...)
		{
			last_error_code = ErrorCode::Failure;
		}

		return last_error_code;
	}

	std::error_code Scheduler::create_operation(size_t operation_id) noexcept
	{
		try
		{
			std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_LOG
			std::cout << "[coyote::create_operation] creating operation " << operation_id << std::endl;
#endif // COYOTE_LOG

			if (!is_attached)
			{
				throw ErrorCode::ClientNotAttached;
			}
			else if (operation_id == main_operation_id)
			{
				throw ErrorCode::MainOperationExplicitlyCreated;
			}

			create_operation(operation_id, lock);
		}
		catch (ErrorCode error_code)
		{
			last_error_code = error_code;
		}
		catch (...)
		{
			last_error_code = ErrorCode::Failure;
		}

		return last_error_code;
	}

	void Scheduler::create_operation(size_t operation_id, std::unique_lock<std::mutex>& lock)
	{
		auto it = operation_map.find(operation_id);
		if (it != operation_map.end())
		{
			throw ErrorCode::DuplicateOperation;
		}

		auto result = operation_map.insert(std::pair<size_t, std::shared_ptr<Operation>>(
			operation_id, std::make_shared<Operation>(operation_id)));
		if (operation_map.size() == 1)
		{
			// This is the first operation, so schedule it.
			scheduled_operation_id = operation_id;
			result.first->second->is_scheduled = true;
		}

		// Increment the count of created operations that have not yet started.
		pending_start_operation_count += 1;
	}

	std::error_code Scheduler::start_operation(size_t operation_id) noexcept
	{
		try
		{
			std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_LOG
			std::cout << "[coyote::start_operation] starting operation " << operation_id << std::endl;
#endif // COYOTE_LOG

			if (!is_attached)
			{
				throw ErrorCode::ClientNotAttached;
			}
			else if (operation_id == main_operation_id)
			{
				throw ErrorCode::MainOperationExplicitlyStarted;
			}

			start_operation(operation_id, lock);
		}
		catch (ErrorCode error_code)
		{
			last_error_code = error_code;
		}
		catch (...)
		{
			last_error_code = ErrorCode::Failure;
		}

		return last_error_code;
	}

	void Scheduler::start_operation(size_t operation_id, std::unique_lock<std::mutex>& lock)
	{
		// TODO: Check pending counter was incremented.

		auto it = operation_map.find(operation_id);
		if (it == operation_map.end())
		{
#ifdef COYOTE_LOG
			std::cout << "[coyote::start_operation] not existing operation " << operation_id << std::endl;
#endif // COYOTE_LOG
			throw ErrorCode::NotExistingOperation;
		}

		std::shared_ptr<Operation> op(it->second);
		if (op->status == OperationStatus::Completed)
		{
			throw ErrorCode::OperationAlreadyCompleted;
		}
		else if (op->status != OperationStatus::None)
		{
			throw ErrorCode::OperationAlreadyStarted;
		}

		// Decrement the count of pending operations.
		pending_start_operation_count -= 1;
#ifdef COYOTE_LOG
		std::cout << "[coyote::start_operation] " << pending_start_operation_count << " operations pending" << std::endl;
#endif // COYOTE_LOG
		if (pending_start_operation_count == 0)
		{
			// If no pending operations remain, then release schedule next.
			pending_operations_cv.notify_all();
		}

		if (op->status != OperationStatus::Completed)
		{
			op->status = OperationStatus::Enabled;
			enabled_operation_ids.push_back(op->id);
			op->cv.notify_all();
			while (!op->is_scheduled)
			{
#ifdef COYOTE_LOG
				std::cout << "[coyote::start_operation] pausing operation " << operation_id << std::endl;
#endif // COYOTE_LOG
				op->cv.wait(lock);
#ifdef COYOTE_LOG
				std::cout << "[coyote::start_operation] resuming operation " << operation_id << std::endl;
#endif // COYOTE_LOG
			}
		}
	}

	std::error_code Scheduler::join_operation(size_t operation_id) noexcept
	{
		try
		{
			std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_LOG
			std::cout << "[coyote::join_operation] joining operation " << operation_id << std::endl;
#endif // COYOTE_LOG

			if (!is_attached)
			{
				throw ErrorCode::ClientNotAttached;
			}

			auto it = operation_map.find(operation_id);
			if (it == operation_map.end())
			{
#ifdef COYOTE_LOG
				std::cout << "[coyote::join_operation] not existing operation " << operation_id << std::endl;
#endif // COYOTE_LOG
				throw ErrorCode::NotExistingOperation;
			}

			std::shared_ptr<Operation> join_op(it->second);
			if (join_op->status != OperationStatus::Completed)
			{
				join_op->blocked_operation_ids.insert(scheduled_operation_id);

				std::shared_ptr<Operation> op(operation_map.at(scheduled_operation_id));
				op->join_operation(operation_id);

				// Waiting for the resource to be released, so schedule the next enabled operation.
				schedule_next(lock);
			}
#ifdef COYOTE_LOG
			else
			{
				std::cout << "[coyote::join_operation] already completed operation " << operation_id << std::endl;
			}
#endif // COYOTE_LOG
		}
		catch (ErrorCode error_code)
		{
			last_error_code = error_code;
		}
		catch (...)
		{
			last_error_code = ErrorCode::Failure;
		}

		return last_error_code;
	}

	std::error_code Scheduler::join_operations(const size_t* operation_ids, size_t size, bool wait_all) noexcept
	{
		try
		{
			std::unique_lock<std::mutex> lock(*mutex);
			if (wait_all)
			{
#ifdef COYOTE_LOG
				std::cout << "[coyote::join_operations] joining all " << size << " operations" << std::endl;
#endif // COYOTE_LOG
			}
			else
			{
#ifdef COYOTE_LOG
				std::cout << "[coyote::join_operations] joining any of " << size << " operations" << std::endl;
#endif // COYOTE_LOG
			}

			if (!is_attached)
			{
				throw ErrorCode::ClientNotAttached;
			}

			std::vector<size_t> join_operations;
			for (int i = 0; i < size; i++)
			{
				size_t operation_id = *(operation_ids + i);
				auto it = operation_map.find(operation_id);
				if (it == operation_map.end())
				{
#ifdef COYOTE_LOG
					std::cout << "[coyote::join_operations] not existing operation " << operation_id << std::endl;
#endif // COYOTE_LOG
					throw ErrorCode::NotExistingOperation;
				}

				std::shared_ptr<Operation> join_op(it->second);
				if (join_op->status != OperationStatus::Completed)
				{
					join_op->blocked_operation_ids.insert(scheduled_operation_id);
					join_operations.push_back(operation_id);
				}
#ifdef COYOTE_LOG
				else
				{
					std::cout << "[coyote::join_operation] already completed operation " << operation_id << std::endl;
				}
#endif // COYOTE_LOG
			}

			if (!join_operations.empty())
			{
				std::shared_ptr<Operation> op(operation_map.at(scheduled_operation_id));
				op->join_operations(join_operations, wait_all);

				// Waiting for the resources to be released, so schedule the next enabled operation.
				schedule_next(lock);
			}
		}
		catch (ErrorCode error_code)
		{
			last_error_code = error_code;
		}
		catch (...)
		{
			last_error_code = ErrorCode::Failure;
		}

		return last_error_code;
	}

	std::error_code Scheduler::complete_operation(size_t operation_id) noexcept
	{
		// TODO: disallow completing main operation.

		try
		{
			std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_LOG
			std::cout << "[coyote::complete_operation] completing operation " << operation_id << std::endl;
#endif // COYOTE_LOG

			if (!is_attached)
			{
				throw ErrorCode::ClientNotAttached;
			}
			else if (operation_id == main_operation_id)
			{
				throw ErrorCode::MainOperationExplicitlyCompleted;
			}

			auto it = operation_map.find(operation_id);
			if (it == operation_map.end())
			{
#ifdef COYOTE_LOG
				std::cout << "[coyote::complete_operation] not existing operation " << operation_id << std::endl;
#endif // COYOTE_LOG
				throw ErrorCode::NotExistingOperation;
			}

			std::shared_ptr<Operation> op(it->second);
			if (op->status == OperationStatus::Completed)
			{
				throw ErrorCode::OperationAlreadyCompleted;
			}
			else if (op->status == OperationStatus::None)
			{
				throw ErrorCode::OperationNotStarted;
			}

			op->status = OperationStatus::Completed;

			// Notify any operations that are waiting to join this operation.
			for (const auto& blocked_id : op->blocked_operation_ids)
			{
				std::shared_ptr<Operation> blocked_op(operation_map.at(blocked_id));
				if (blocked_op->on_join_operation(operation_id))
				{
					enabled_operation_ids.push_back(blocked_op->id);
				}
			}

			// The current operation has completed, so schedule the next enabled operation.
			schedule_next(lock);
		}
		catch (ErrorCode error_code)
		{
			last_error_code = error_code;
		}
		catch (...)
		{
			last_error_code = ErrorCode::Failure;
		}

		return last_error_code;
	}

	std::error_code Scheduler::create_resource(size_t resource_id) noexcept
	{
		try
		{
			std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_LOG
			std::cout << "[coyote::create_resource] creating resource " << resource_id << std::endl;
#endif // COYOTE_LOG

			if (!is_attached)
			{
				throw ErrorCode::ClientNotAttached;
			}

			auto it = resource_map.find(resource_id);
			if (it != resource_map.end())
			{
				throw ErrorCode::DuplicateResource;
			}

			resource_map.insert(std::pair<size_t, std::shared_ptr<std::unordered_set<size_t>>>(
				resource_id, std::make_shared<std::unordered_set<size_t>>()));
		}
		catch (ErrorCode error_code)
		{
			last_error_code = error_code;
		}
		catch (...)
		{
			last_error_code = ErrorCode::Failure;
		}

		return last_error_code;
	}

	std::error_code Scheduler::wait_resource(size_t resource_id) noexcept
	{
		try
		{
			std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_LOG
			std::cout << "[coyote::wait_resource] waiting resource " << resource_id << std::endl;
#endif // COYOTE_LOG

			if (!is_attached)
			{
				throw ErrorCode::ClientNotAttached;
			}

			std::shared_ptr<Operation> op(operation_map.at(scheduled_operation_id));
			op->wait_resource_signal(resource_id);

			auto it = resource_map.find(resource_id);
			if (it == resource_map.end())
			{
				throw ErrorCode::NotExistingResource;
			}

			std::shared_ptr<std::unordered_set<size_t>> blocked_operation_ids(it->second);
			blocked_operation_ids->insert(scheduled_operation_id);

			// Waiting for the resource to be released, so schedule the next enabled operation.
			schedule_next(lock);
		}
		catch (ErrorCode error_code)
		{
			last_error_code = error_code;
		}
		catch (...)
		{
			last_error_code = ErrorCode::Failure;
		}

		return last_error_code;
	}

	std::error_code Scheduler::wait_resources(const size_t* resource_ids, size_t size, bool wait_all) noexcept
	{
		try
		{
			std::unique_lock<std::mutex> lock(*mutex);
			if (wait_all)
			{
#ifdef COYOTE_LOG
				std::cout << "[coyote::wait_resources] waiting all " << size << " resources" << std::endl;
#endif // COYOTE_LOG
			}
			else
			{
#ifdef COYOTE_LOG
				std::cout << "[coyote::wait_resources] waiting any of " << size << " resources" << std::endl;
#endif // COYOTE_LOG
			}

			if (!is_attached)
			{
				throw ErrorCode::ClientNotAttached;
			}

			std::shared_ptr<Operation> op(operation_map.at(scheduled_operation_id));
			op->wait_resource_signals(resource_ids, size, wait_all);

			for (int i = 0; i < size; i++)
			{
				size_t resource_id = *(resource_ids + i);
				auto it = resource_map.find(resource_id);
				if (it == resource_map.end())
				{
					throw ErrorCode::NotExistingResource;
				}

				std::shared_ptr<std::unordered_set<size_t>> blocked_operation_ids(it->second);
				blocked_operation_ids->insert(scheduled_operation_id);
			}

			// Waiting for the resources to be released, so schedule the next enabled operation.
			schedule_next(lock);
		}
		catch (ErrorCode error_code)
		{
			last_error_code = error_code;
		}
		catch (...)
		{
			last_error_code = ErrorCode::Failure;
		}

		return last_error_code;
	}

	std::error_code Scheduler::signal_resource(size_t resource_id) noexcept
	{
		try
		{
			std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_LOG
			std::cout << "[coyote::signal_resource] signaling resource " << resource_id << std::endl;
#endif // COYOTE_LOG

			if (!is_attached)
			{
				throw ErrorCode::ClientNotAttached;
			}

			auto it = resource_map.find(resource_id);
			if (it == resource_map.end())
			{
				throw ErrorCode::NotExistingResource;
			}

			std::shared_ptr<std::unordered_set<size_t>> blocked_operation_ids(it->second);
			for (const auto& blocked_id : *blocked_operation_ids)
			{
				std::shared_ptr<Operation> blocked_op(operation_map.at(blocked_id));
				if (blocked_op->on_resource_signal(resource_id))
				{
					enabled_operation_ids.push_back(blocked_op->id);
				}
			}
		}
		catch (ErrorCode error_code)
		{
			last_error_code = error_code;
		}
		catch (...)
		{
			last_error_code = ErrorCode::Failure;
		}

		return last_error_code;
	}

	std::error_code Scheduler::delete_resource(size_t resource_id) noexcept
	{
		try
		{
			std::unique_lock<std::mutex> lock(*mutex);
#ifdef COYOTE_LOG
			std::cout << "[coyote::delete_resource] deleting resource " << resource_id << std::endl;
#endif // COYOTE_LOG

			if (!is_attached)
			{
				throw ErrorCode::ClientNotAttached;
			}

			auto it = resource_map.find(resource_id);
			if (it == resource_map.end())
			{
				throw ErrorCode::NotExistingResource;
			}

			resource_map.erase(resource_id);
		}
		catch (ErrorCode error_code)
		{
			last_error_code = error_code;
		}
		catch (...)
		{
			last_error_code = ErrorCode::Failure;
		}

		return last_error_code;
	}

	std::error_code Scheduler::schedule_next() noexcept
	{
		try
		{
			std::unique_lock<std::mutex> lock(*mutex);

			if (!is_attached)
			{
				throw ErrorCode::ClientNotAttached;
			}

			schedule_next(lock);
		}
		catch (ErrorCode error_code)
		{
			last_error_code = error_code;
		}
		catch (...)
		{
			last_error_code = ErrorCode::Failure;
		}

		return last_error_code;
	}

	void Scheduler::schedule_next(std::unique_lock<std::mutex>& lock)
	{
#ifdef COYOTE_LOG
		std::cout << "[coyote::schedule_next] current operation " << scheduled_operation_id << std::endl;
#endif // COYOTE_LOG

		// Wait for any recently created operations to start.
		while (pending_start_operation_count > 0)
		{
#ifdef COYOTE_LOG
			std::cout << "[coyote::schedule_next] waiting " << pending_start_operation_count <<
				" pending operations" << std::endl;
#endif // COYOTE_LOG
			pending_operations_cv.wait(lock);
		}

		const size_t previous_id = scheduled_operation_id;
		std::shared_ptr<Operation> previous_op(operation_map.at(previous_id));
		if (previous_op->status == OperationStatus::JoinAllOperations)
		{
#ifdef COYOTE_LOG
			std::cout << "[coyote::schedule_next] current operation " << scheduled_operation_id << std::endl;
#endif // COYOTE_LOG
		}

		// Check if there are any enabled operations or if the client has deadlocked.
		check_operation_availability();

		enabled_operation_ids.erase(std::remove_if(enabled_operation_ids.begin(), enabled_operation_ids.end(),
			[this](size_t id) { return operation_map[id]->status != OperationStatus::Enabled; }), enabled_operation_ids.end());

		// Ask the strategy for the next operation to schedule.
		size_t next_id = strategy->next_operation(enabled_operation_ids);
		std::shared_ptr<Operation> next_op(operation_map.at(next_id));
		scheduled_operation_id = next_id;

#ifdef COYOTE_LOG
		std::cout << "[coyote::schedule_next] next operation " << next_id << std::endl;
#endif // COYOTE_LOG

		if (previous_id != next_id)
		{
			// Resume the next operation.
			next_op->is_scheduled = true;
			next_op->cv.notify_all();

			// Pause the previous operation.
			if (previous_op->status != OperationStatus::Completed)
			{
				//std::unique_lock<std::mutex> lock(*mutex);
				previous_op->is_scheduled = false;

				while (!previous_op->is_scheduled)
				{
#ifdef COYOTE_LOG
					std::cout << "[coyote::schedule_next] pausing operation " << previous_id << std::endl;
#endif // COYOTE_LOG
					// Wait until the operation gets scheduled again.
					previous_op->cv.wait(lock);
#ifdef COYOTE_LOG
					std::cout << "[coyote::schedule_next] resuming operation " << previous_id << std::endl;
#endif // COYOTE_LOG
				}
			}
		}
	}

	void Scheduler::check_operation_availability()
	{
		int blocked_operations = 0;
		bool exists_enabled_operation = false;
		for (auto& op : operation_map)
		{
			if (op.second->status == OperationStatus::Enabled)
			{
				exists_enabled_operation = true;
				break;
			}
			else if (op.second->status == OperationStatus::JoinAllOperations ||
				op.second->status == OperationStatus::JoinAnyOperations ||
				op.second->status == OperationStatus::WaitAllResources ||
				op.second->status == OperationStatus::WaitAnyResource)
			{
				blocked_operations += 1;
			}
		}

		if (!exists_enabled_operation)
		{
			if (blocked_operations > 0)
			{
#ifdef COYOTE_LOG
				std::cout << "[coyote::schedule_next] deadlock detected" << std::endl;
#endif // COYOTE_LOG
				throw ErrorCode::DeadlockDetected;
			}

#ifdef COYOTE_LOG
			std::cout << "[coyote::schedule_next] no enabled operation to schedule" << std::endl;
#endif // COYOTE_LOG
			throw ErrorCode::Success;
		}
	}

	bool Scheduler::next_boolean() noexcept
	{
#ifdef COYOTE_LOG
		std::cout << "[coyote::next_boolean] " << std::endl;
#endif // COYOTE_LOG
		return strategy->next_boolean();
	}

	int Scheduler::next_integer(int max_value) noexcept
	{
#ifdef COYOTE_LOG
		std::cout << "[coyote::next_integer] " << std::endl;
#endif // COYOTE_LOG
		return strategy->next_integer(max_value);
	}

	size_t Scheduler::seed() noexcept
	{
		return strategy->seed();
	}

	std::error_code Scheduler::error_code() noexcept
	{
		return last_error_code;
	}
}
