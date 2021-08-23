#include <atomic>
#include <vector>
#include <cstdint>

#include "atomicactions.h"

#ifndef STATE
#define STATE
#include "state.h"
#endif

GlobalState *global_state = nullptr;

/**
 *
 **/
value choose_random(std::vector<value> *rf_set) {  
  return (*rf_set)[0];
}


/**
 *
 **/
void atomic_init(location store_address, value init_value = 0, thread_id tid = 0) {
  if(global_state == nullptr) {
    initialise_global_state();
  }
  
  Store *s = new Store(store_address, init_value, std::memory_order_seq_cst , tid);
  s->execute();
}


/**
 *
 **/
value atomic_load(location load_address, memory_order load_order , thread_id tid) {
  if(global_state == nullptr) {
    initialise_global_state();
  }
  Load *l = new Load(load_address, load_order, tid);
  l->execute();
  return choose_random(l->get_rf_set());
}


/**
 *
 **/
void atomic_store(location store_address, value store_value, memory_order store_order, \
		  thread_id tid) {
  if(global_state == nullptr) {
    initialise_global_state();
  }
  Store *s = new Store(store_address, store_value, store_order, tid);
  s->execute();
}


/**
 * @brief models generic compare and exchange operation
 *
 * Ex : atomic_compare_exchange_weak, atomic_compare_exchange_strong
 **/
value atomic_rmw(location load_store_address, value expected, memory_order success_order, \
		 value desired,memory_order failure_order, thread_id tid) {
  if(global_state == nullptr) {
    initialise_global_state();
  }
  
  RMW *rmw = new RMW(load_store_address, expected, success_order, desired, \
		     failure_order, tid);
  rmw->execute();
  return rmw->get_return_value();
}


/**
 *
 *
 * 
 **/
value atomic_fetch_add(location load_store_address,value operand,
		      memory_order success_order,
		      thread_id tid){
  if(global_state == nullptr) {
    initialise_global_state();
  }

  FetchOp *fop = new FetchOp(load_store_address, operand, success_order,
			     tid, binary_op::add_op);
  fop->execute();
  return fop->get_return_value();
}


/**
 *
 *
 *
 **/
value atomic_fetch_sub(location load_store_address,value operand,
		      memory_order success_order,
		      thread_id tid) {
  if(global_state == nullptr) {
    initialise_global_state();
  }

  FetchOp *fop = new FetchOp(load_store_address, operand, success_order,
			     tid, binary_op::sub_op);
  fop->execute();
  return fop->get_return_value();

}


/**
 *
 *
 *
 **/
value atomic_fetch_and(location load_store_address,value operand,
		      memory_order success_order,
		      thread_id tid) {
  if(global_state == nullptr) {
    initialise_global_state();
  }
  
  FetchOp *fop = new FetchOp(load_store_address, operand, success_order,
			     tid, binary_op::and_op);
  fop->execute();
  return fop->get_return_value();

}


/**
 *
 *
 *
 **/
value atomic_fetch_or(location load_store_address,value operand,
		      memory_order success_order,
		      thread_id tid) {
  if(global_state == nullptr) {
    initialise_global_state();
  }

  FetchOp *fop = new FetchOp(load_store_address, operand, success_order,
			     tid, binary_op::or_op);
  fop->execute();
  return fop->get_return_value();

}


/**
 *
 *
 *
 **/
value atomic_fetch_xor(location load_store_address,value operand,
		      memory_order success_order,
		      thread_id tid) {
  if(global_state == nullptr) {
    initialise_global_state();
  }

  FetchOp *fop = new FetchOp(load_store_address, operand, success_order,
			     tid, binary_op::xor_op);
  fop->execute();
  return fop->get_return_value();
}



/**
 *
 *
 *
 **/
void atomic_fence(memory_order fence_order, thread_id tid) {
  if(global_state == nullptr) {
    initialise_global_state();
  }
  
  Fence *fence = new Fence(fence_order, tid);
  fence->execute();
}
