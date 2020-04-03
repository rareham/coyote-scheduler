// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <iostream>
#include <vector>
#include "scheduler.h"

using namespace coyote;

Scheduler::Scheduler() noexcept :
	mutex(std::make_unique<std::mutex>()),
	pending_operations_cv(),
	scheduled_operation_id(0),
	pending_operation_count(0),
	is_attached(false),
	error_code(ErrorCode::Success)
{
}

std::error_code Scheduler::attach()
{
	try
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifndef NDEBUG
		std::cout << "[coyote::attach] attaching the current operation" << std::endl;
#endif // NDEBUG

		if (is_attached)
		{
			throw ErrorCode::ClientAttached;
		}

		is_attached = true;
		error_code = ErrorCode::Success;

		// Increment the count of pending operations.
		pending_operation_count++;

#pragma warning (disable : 26441)
		lock.swap(start_operation(main_operation_id, std::move(lock)));
#pragma warning (restore : 26441)
	}
	catch (ErrorCode ex)
	{
		error_code = ex;
	}
	catch (...)
	{
		error_code = ErrorCode::Failure;
	}

	return error_code;
}

std::error_code Scheduler::detach()
{
	try
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifndef NDEBUG
		std::cout << "[coyote::detach] releasing all operations" << std::endl;
#endif // NDEBUG

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
#ifndef NDEBUG
				std::cout << "[coyote::detach] canceling operation " << op->id << std::endl;
#endif // NDEBUG
				// If the operation has not already completed, then cancel it.
				op->is_scheduled = true;
				op->status = OperationStatus::Canceled;
				op->cv.notify_all();
			}
		}

		operation_map.clear();
		resource_map.clear();
		pending_operation_count = 0;
	}
	catch (ErrorCode ex)
	{
		error_code = ex;
	}
	catch (...)
	{
		error_code = ErrorCode::Failure;
	}

	return error_code;
}

std::error_code Scheduler::create_next_operation()
{
	try
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifndef NDEBUG
		std::cout << "[coyote::create_next_operation] creating next operation" << std::endl;
#endif // NDEBUG

		if (!is_attached)
		{
			throw ErrorCode::ClientNotAttached;
		}

		// Increment the count of pending operations.
		pending_operation_count++;
	}
	catch (ErrorCode ex)
	{
		error_code = ex;
	}
	catch (...)
	{
		error_code = ErrorCode::Failure;
	}

	return error_code;
}

std::error_code Scheduler::start_operation(size_t operation_id)
{
	try
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifndef NDEBUG
		std::cout << "[coyote::start_operation] starting operation " << operation_id << std::endl;
#endif // NDEBUG

		if (!is_attached)
		{
			throw ErrorCode::ClientNotAttached;
		}
		else if (operation_id == main_operation_id)
		{
			throw ErrorCode::ExplicitMainOperationStart;
		}

#pragma warning (push)
#pragma warning (disable : 26441)
		lock.swap(start_operation(operation_id, std::move(lock)));
#pragma warning (pop)
	}
	catch (ErrorCode ex)
	{
		error_code = ex;
	}
	catch (...)
	{
		error_code = ErrorCode::Failure;
	}

	return error_code;
}

std::unique_lock<std::mutex> Scheduler::start_operation(size_t operation_id, std::unique_lock<std::mutex> lock)
{
	// TODO: Check pending counter was incremented.

	auto it = operation_map.find(operation_id);
	if (it != operation_map.end())
	{
		throw ErrorCode::DuplicateOperationStart;
	}

	auto result = operation_map.insert(std::pair<size_t, std::shared_ptr<Operation>>(
		operation_id, std::make_shared<Operation>(operation_id)));
	Operation* op = result.first->second.get();
	if (operation_map.size() == 1)
	{
		// This is the first operation, so schedule it.
		scheduled_operation_id = operation_id;
		op->is_scheduled = true;
	}

	// Decrement the count of pending operations.
	pending_operation_count--;
#ifndef NDEBUG
	std::cout << "[coyote::start_operation] " << pending_operation_count << " operations pending" << std::endl;
#endif // NDEBUG
	if (pending_operation_count == 0)
	{
#ifndef NDEBUG
		std::cout << "[coyote::start_operation] no pending operations" << std::endl;
#endif // NDEBUG

		// If no pending operations remain, then release schedule next.
		pending_operations_cv.notify_all();
	}

	if (op->status != OperationStatus::Canceled)
	{
		op->status = OperationStatus::Enabled;
		op->cv.notify_all();
		while (!op->is_scheduled)
		{
#ifndef NDEBUG
			std::cout << "[coyote::start_operation] pausing operation " << operation_id << std::endl;
#endif // NDEBUG
			op->cv.wait(lock);
#ifndef NDEBUG
			std::cout << "[coyote::start_operation] resuming operation " << operation_id << std::endl;
#endif // NDEBUG
		}
	}

	return lock;
}

std::error_code Scheduler::complete_operation(size_t operation_id)
{
	// TODO: disallow completing main operation.

	try
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifndef NDEBUG
		std::cout << "[coyote::complete_operation] completing operation " << operation_id << std::endl;
#endif // NDEBUG

		if (!is_attached)
		{
			throw ErrorCode::ClientNotAttached;
		}
		else if (operation_id == main_operation_id)
		{
			throw ErrorCode::ExplicitMainOperationComplete;
		}

		std::shared_ptr<Operation> op(operation_map.at(operation_id));
		op->status = OperationStatus::Completed;
		operation_map.erase(operation_id);

#pragma warning(push)
#pragma warning (disable : 26441)
		// The current operation has completed, so schedule the next enabled operation.
		lock.swap(schedule_next_operation(std::move(lock)));
#pragma warning (pop)
	}
	catch (ErrorCode ex)
	{
		error_code = ex;
	}
	catch (...)
	{
		error_code = ErrorCode::Failure;
	}

	return error_code;
}

std::error_code Scheduler::create_resource(size_t resource_id)
{
	try
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifndef NDEBUG
		std::cout << "[coyote::create_resource] creating resource " << resource_id << std::endl;
#endif // NDEBUG

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
	catch (ErrorCode ex)
	{
		error_code = ex;
	}
	catch (...)
	{
		error_code = ErrorCode::Failure;
	}

	return error_code;
}

std::error_code Scheduler::wait_resource(size_t resource_id)
{
	try
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifndef NDEBUG
		std::cout << "[coyote::wait_resource] waiting resource " << resource_id << std::endl;
#endif // NDEBUG

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

#pragma warning(push)
#pragma warning (disable : 26441)
		// Waiting for the resource to be released, so schedule the next enabled operation.
		lock.swap(schedule_next_operation(std::move(lock)));
#pragma warning (pop)
	}
	catch (ErrorCode ex)
	{
		error_code = ex;
	}
	catch (...)
	{
		error_code = ErrorCode::Failure;
	}

	return error_code;
}

std::error_code Scheduler::wait_resources(const size_t* resource_ids, int array_size, bool wait_all)
{
	try
	{
		std::unique_lock<std::mutex> lock(*mutex);
		if (wait_all)
		{
#ifndef NDEBUG
			std::cout << "[coyote::wait_resources] waiting all " << array_size << " resources" << std::endl;
#endif // NDEBUG
		}
		else
		{
#ifndef NDEBUG
			std::cout << "[coyote::wait_resources] waiting any of " << array_size << " resources" << std::endl;
#endif // NDEBUG
		}

		if (!is_attached)
		{
			throw ErrorCode::ClientNotAttached;
		}

		std::shared_ptr<Operation> op(operation_map.at(scheduled_operation_id));
		op->wait_resource_signals(resource_ids, array_size, wait_all);

		for (int i = 0; i < array_size; i++)
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

#pragma warning(push)
#pragma warning (disable : 26441)
		// Waiting for the resources to be released, so schedule the next enabled operation.
		lock.swap(schedule_next_operation(std::move(lock)));
#pragma warning (pop)
	}
	catch (ErrorCode ex)
	{
		error_code = ex;
	}
	catch (...)
	{
		error_code = ErrorCode::Failure;
	}

	return error_code;
}

std::error_code Scheduler::signal_resource(size_t resource_id)
{
	try
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifndef NDEBUG
		std::cout << "[coyote::signal_resource] signaling resource " << resource_id << std::endl;
#endif // NDEBUG

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
		for (const auto& operation_id : *blocked_operation_ids)
		{
			std::shared_ptr<Operation> op(operation_map.at(operation_id));
			op->on_resource_signal(resource_id);
		}
	}
	catch (ErrorCode ex)
	{
		error_code = ex;
	}
	catch (...)
	{
		error_code = ErrorCode::Failure;
	}

	return error_code;
}

std::error_code Scheduler::delete_resource(size_t resource_id)
{
	try
	{
		std::unique_lock<std::mutex> lock(*mutex);
#ifndef NDEBUG
		std::cout << "[coyote::delete_resource] deleting resource " << resource_id << std::endl;
#endif // NDEBUG

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
	catch (ErrorCode ex)
	{
		error_code = ex;
	}
	catch (...)
	{
		error_code = ErrorCode::Failure;
	}

	return error_code;
}

std::error_code Scheduler::schedule_next_operation()
{
	try
	{
		std::unique_lock<std::mutex> lock(*mutex);

		if (!is_attached)
		{
			throw ErrorCode::ClientNotAttached;
		}

#pragma warning(push)
#pragma warning (disable : 26441)
		lock.swap(schedule_next_operation(std::move(lock)));
#pragma warning (pop)
	}
	catch (ErrorCode ex)
	{
		error_code = ex;
	}
	catch (...)
	{
		error_code = ErrorCode::Failure;
	}

	return error_code;
}

std::unique_lock<std::mutex> Scheduler::schedule_next_operation(std::unique_lock<std::mutex> lock)
{
#ifndef NDEBUG
	std::cout << "[coyote::schedule_next_operation] current operation " << scheduled_operation_id << std::endl;
#endif // NDEBUG

	// Wait for any recently created operations to start.
	while (pending_operation_count > 0)
	{
#ifndef NDEBUG
		std::cout << "[coyote::schedule_next_operation] waiting " << pending_operation_count <<
			" pending operations" << std::endl;
#endif // NDEBUG
		pending_operations_cv.wait(lock);
#ifndef NDEBUG
		std::cout << "[coyote::schedule_next_operation] waited " << pending_operation_count <<
			" pending operations" << std::endl;
#endif // NDEBUG

	}

	size_t previous_id = scheduled_operation_id;
	std::shared_ptr<Operation> previous_op(operation_map.at(previous_id));

	size_t next_id{};
	std::shared_ptr<Operation> next_op;

	// TODO: replace with strategy.
	for (auto& op : operation_map)
	{
		if (op.second->status == OperationStatus::Enabled)
		{
			scheduled_operation_id = next_id = op.first;
			next_op.swap(std::shared_ptr<Operation>(op.second));
			break;
		}
	}

	if (!next_op)
	{
		throw ErrorCode::CompletedAllOperations;
	}

#ifndef NDEBUG
	std::cout << "[coyote::schedule_next_operation] next operation " << next_id << std::endl;
#endif // NDEBUG

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
#ifndef NDEBUG
				std::cout << "[coyote::schedule_next_operation] pausing operation " << previous_id << std::endl;
#endif // NDEBUG
				// Wait until the operation gets scheduled again.
				previous_op->cv.wait(lock);
#ifndef NDEBUG
				std::cout << "[coyote::schedule_next_operation] resuming operation " << previous_id << std::endl;
#endif // NDEBUG
			}
		}
	}

	return lock;
}

bool Scheduler::get_next_boolean()
{
#ifndef NDEBUG
	std::cout << "[coyote::get_next_boolean] " << std::endl;
#endif // NDEBUG
	return false;
}

size_t Scheduler::get_next_integer(size_t max_value)
{
#ifndef NDEBUG
	std::cout << "[coyote::get_next_integer] " << std::endl;
#endif // NDEBUG
	return 7;
}

std::error_code Scheduler::get_last_error_code()
{
	return error_code;
}
