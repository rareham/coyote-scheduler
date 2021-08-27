#ifndef STATE
#define STATE
#include "atomicstate.h"
#endif

void atomic_init(thread_id, value, location);

value atomic_load(thread_id, location, operation_order);

void atomic_store(thread_id, value, location, operation_order);

value atomic_rmw(thread_id, value, value, location, operation_order, operation_order);

value atomic_fetch_add(thread_id, value, location, operation_order);

value atomic_fetch_sub(thread_id, value, location, operation_order);

value atomic_fetch_and(thread_id, value, location, operation_order);

value atomic_fetch_or(thread_id, value, location, operation_order);

value atomic_fetch_xor(thread_id, value, location, operation_order);

void atomic_fence(thread_id, operation_order);
