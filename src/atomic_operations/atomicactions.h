#ifndef STATE
#define STATE
#include "state.h"
#endif

void atomic_init(location , value , thread_id);
  
Operation* choose_random(std::vector<Operation*>);

uint64_t atomic_load(location, memory_order, thread_id);

void atomic_store(location, value, memory_order, thread_id);

uint64_t atomic_rmw(location, value, memory_order, value, memory_order, thread_id);

void atomic_fence(memory_order, thread_id);
