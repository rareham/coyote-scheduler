// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <iostream>
#include "operation.h"

using namespace coyote;

Operation::Operation(size_t operation_id) noexcept :
	cv(),
	id(operation_id),
	status(OperationStatus::None),
	is_scheduled(false)
{
}

void Operation::wait_resource_signal(size_t resource_id)
{
	status = OperationStatus::WaitAllResources;
	pending_resource_ids.insert(resource_id);
}

void Operation::wait_resource_signals(const size_t* resource_ids, int array_size, bool wait_all)
{
	if (wait_all)
	{
		status = OperationStatus::WaitAllResources;
	}
	else
	{
		status = OperationStatus::WaitAnyResource;
	}

	for (int i = 0; i < array_size; i++)
	{
		pending_resource_ids.insert(*(resource_ids + i));
	}
}

void Operation::on_resource_signal(size_t resource_id)
{
	pending_resource_ids.erase(resource_id);
	if (status == OperationStatus::WaitAllResources && pending_resource_ids.empty())
	{
		// If the operation is waiting for a signal from all resources, and there
		// are no more resources to signal, then enable the operation.
		status = OperationStatus::Enabled;
	}
	else if (status == OperationStatus::WaitAnyResource)
	{
		// If the operation is waiting for at least one signal, then enable the operation,
		// and clear the set of pending resource signals.
		status = OperationStatus::Enabled;
		pending_resource_ids.clear();
	}
}
