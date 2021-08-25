#ifndef STATE
#define STATE
#include "state.h"
#endif

void atomic_init(location, value, thread_id);

uint64_t atomic_load(location, operation_order, thread_id);

void atomic_store(location, value, operation_order, thread_id);

uint64_t atomic_rmw(location, value, operation_order, value, operation_order, thread_id);

value atomic_fetch_add(location, value, operation_order, thread_id);

value atomic_fetch_sub(location, value, operation_order, thread_id);

value atomic_fetch_and(location, value, operation_order, thread_id);

value atomic_fetch_or(location, value, operation_order, thread_id);

value atomic_fetch_xor(location, value, operation_order, thread_id);

void atomic_fence(operation_order, thread_id);
