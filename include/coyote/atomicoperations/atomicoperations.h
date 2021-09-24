#pragma once

#include "atomicmodel.h"
#include <iostream>
#include <cstdint>

#include "atomicstate.h"
#include "../scheduler.h"

#define ASSERT_CHECK //if(assert_failed)	\
                     {\
		       assert(false, "Assertion Failed");	\
		       global_state->print_execution_trace();\
		     }\


/**
 * @brief models atomic init operation
 * 
 **/
template<typename T>
void atomic_init(atomic<T>* store_address, T init_value)
{
  #ifdef ATOMIC_MODEL_DEBUG
  std::cout << "Started Initing"
	    << std::endl;
  #endif
  thread_id tid = global_state->get_scheduled_op_id();
  
  location loc = (uintptr_t) store_address;
  STR<T> *s = new STR<T>(loc, init_value, operation_order::relaxed , tid);
  s->execute();

  #ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_init :: initialised at location : " << store_address
	    << " with value : " << init_value
	    << " by thread : " << tid
	    << std::endl;
  #endif
}


/**
 * @brief models atomic load operation
 * 
 **/
template<typename T>
T atomic_load(atomic<T>* load_address, operation_order load_order)
{
  coyote::Scheduler *scheduler = global_state->get_scheduler();

  scheduler->schedule_next();
  
  #ifdef ATOMIC_MODEL_DEBUG
  std::cout << "Started LDing ..." << std::endl;
  #endif

  thread_id tid = global_state->get_scheduled_op_id();
  location loc = (uintptr_t) load_address;
  LD<T> *l = new LD<T>(loc, load_order, tid);
  T ret_val = l->execute();

  #ifdef ATOMIC_MODEL_DEBUG
  std::cout << "atomic_load :: load from location : " << load_address
	    << " with order : " << load_order
	    << " by thread : " << tid
	    << std::endl;
  #endif

  return ret_val;
}




/**
 * @brief models atomic store operation
 *
 **/
template<typename T>
void atomic_store(atomic<T>* store_address, T store_value, 
		  operation_order store_order)
{
  #ifdef ATOMIC_MODEL_DEBUG
  std::cout << "Storing ..." << std::endl;
  #endif

  thread_id tid = global_state->get_scheduled_op_id();
  location loc = (uintptr_t) store_address;
  
  STR<T> *s = new STR<T>(loc, store_value, store_order, tid);
  s->execute();

  #ifdef ATOMIC_MODEL_DEBUG
  std::cout << "atomic_store :: stored at location : " << store_address
	    << " with value : " << store_value
	    << " store_order : " << store_order
	    << " by thread : " << tid
	    << std::endl;
  #endif
  global_state->print_thread_stores();

}



/**
 * @brief models atomic rmw operations
 *
 **/
template<typename T>
bool atomic_rmw(atomic<T>* load_store_address,T expected, T desired,
		operation_order success_order ,
		operation_order failure_order )
{
  #ifdef ATOMIC_MODEL_DEBUG
  std::cout << "RMWing ..." << std::endl;
  #endif
  
  thread_id tid = global_state->get_scheduled_op_id();

  location loc = (uintptr_t) load_store_address;  

  RMW<T> *rmw = new RMW<T>(loc, expected, success_order, desired,
			   failure_order, tid);
  rmw->execute();
    

  #ifdef ATOMIC_MODEL_DEBUG
  std::cout << "atomic_rmw :: rmw at location : " << load_store_address
	    << " with expected_value : " << expected
	    << " success_order : " << success_order
	    << " with desired_value : " << desired
	    << " failure_order : " << failure_order
	    << " by thread : " << tid
	    << std::endl;
  #endif

  bool ret_val = rmw->get_return_value();

  return ret_val;
}



/**
 * @brief models atomic fetch and binary operation(rmw)
 *
 **/
template<typename T>
T atomic_fetch_op(atomic<T>* load_store_address, value operand,
		  operation_order memory_order, binary_op bop)
{
  #ifdef ATOMIC_MODEL_DEBUG
  std::cout << "FOP adding ..." << std::endl;
  #endif

  thread_id tid = global_state->get_scheduled_op_id();
  
  location loc = (uintptr_t) load_store_address;
  
  FOP<T> *fop = new FOP<T>(loc, operand, memory_order,
				   tid, bop);
  fop->execute();

  T ret_val = fop->get_return_value();
  
#ifdef ATOMIC_MODEL_DEBUG
  std::cout << "atomic_fetch_op :: stored at location : " << load_store_address
	    << " with value : " << ret_val
	    << " binary op : " << bop
	    << " order : " << memory_order
	    << " by thread : " << tid
	    << std::endl;
#endif

  return ret_val; 
}


/**
 * @brief models atomic exchange
 *
 **/
template<typename T>
T atomic_exchange(atomic<T>* load_store_address, T exchange,
		  operation_order memory_order)
{
#ifdef ATOMIC_MODEL_DEBUG
  std::cout << "Exchanging ..." << std::endl;
#endif

  thread_id tid = global_state->get_scheduled_op_id();

  location loc = (uintptr_t) load_store_address;

  XCHNG<T> *xchng = new XCHNG<T>(loc, memory_order, exchange, tid);

  xchng->execute();
  
  T ret_val = xchng->get_return_value();

  #ifdef ATOMIC_MODEL_DEBUG
  std::cout << "atomic_exchange :: stored at location : " << load_store_address
	    << " with value : " << ret_val << " order : " << memory_order
	    << " by thread : " << tid << std::endl;
 #endif

  return ret_val;
}


/**
 * @brief models atomic fence
 *
 **/
void atomic_fence(operation_order fence_order )
{
  #ifdef ATOMIC_MODEL_DEBUG
  std::cout << "Fencing...." << std::endl;
  #endif

  thread_id tid = global_state->get_scheduled_op_id();

  Fence *fence = new Fence(fence_order, tid);
  fence->execute();
  #ifdef ATOMIC_MODEL_DEBUG
  std::cout << "atomic_fence :: order : " << fence_order
	    << " by thread : " << tid
	    << std::endl;
  #endif
}

template<typename T>
void atomic<T>::store(T store_value,  operation_order store_order ) 
{
  atomic_store(this, store_value, store_order);
}

template<typename T>
T atomic<T>::load(operation_order oper_order ) 
{
  return atomic_load(this, oper_order);
}

template<typename T>
T atomic<T>::exchange(T exchange, operation_order oper_order) 
{
  return atomic_exchange(this, exchange, oper_order);
}

template<typename T>
T atomic<T>::exchange(T exchange) 
{
  return atomic_exchange(this, exchange, memory_order_seq_cst);
}

template<typename T>
bool atomic<T>::compare_exchange_weak(T expected, T desired,
				      operation_order success_order,
				      operation_order failure_order ) 
{
  return atomic_rmw(this, expected, desired, success_order, failure_order);
}

template<typename T>
bool atomic<T>::compare_exchange_strong(T expected, T desired,
					operation_order success_order,
					operation_order failure_order) 
{
  return atomic_rmw(this, expected, desired, success_order, failure_order);
}


template<typename T>
bool atomic<T>::compare_exchange_strong(T expected, T desired,
					operation_order success_order) 
{
  operation_order failure_order;
  
  if (success_order == operation_order::acq_rel)
    {
      failure_order = operation_order::acquire;
    }
  else if (success_order == operation_order::release)
    {
      failure_order = operation_order::relaxed;
    }
  
  return atomic_rmw(this, expected, desired, success_order, failure_order);
}


template<typename T>
T atomic<T>::fetch_add(T operand, operation_order oper_order)
{
  return atomic_fetch_op(this, operand, oper_order, binary_op::add_op);
}

template<typename T>
T atomic<T>::fetch_sub(T operand, operation_order oper_order)
{
  return atomic_fetch_op(this, operand, oper_order, binary_op::sub_op);
}

template<typename T>
T atomic<T>::fetch_and(T operand, operation_order oper_order)
{
  return atomic_fetch_op(this, operand, oper_order, binary_op::and_op);
}

template<typename T>
T atomic<T>::fetch_or(T operand, operation_order oper_order)
{
  return atomic_fetch_op(this, operand, oper_order, binary_op::or_op);
}

template<typename T>
T atomic<T>::fetch_xor(T operand, operation_order oper_order)
{
  return atomic_fetch_op(this, operand, oper_order, binary_op::xor_op);
}

template<typename T>
T atomic<T>::operator =( T str_value )
{
  atomic_store(this, str_value, operation_order::seq_cst);
  return str_value;
}

template<typename T>
T atomic<T>::operator ++()
{
  return atomic_fetch_op(this, 1, operation_order::seq_cst, binary_op::add_op);
}

template<typename T>
T atomic<T>::operator --()
{
  return atomic_fetch_op(this, 1, operation_order::seq_cst, binary_op::sub_op);
}

template<typename T>
T atomic<T>::operator +=(T operand) 
{
  return atomic_fetch_op(this, operand, operation_order::seq_cst,
			 binary_op::add_op);
}

template<typename T>
T atomic<T>::operator -=(T operand) 
{
  return atomic_fetch_op(this, operand, operation_order::seq_cst,
			 binary_op::sub_op);
}

template<typename T>
T atomic_load_explicit(atomic<T> *load_address, operation_order load_order)
{
  return atomic_load(load_address, load_order);
}

template<typename T>
void atomic_store_explicit(atomic<T> *store_address, T value,
			   operation_order store_order)
{
  atomic_store(store_address, value, store_order);
}

template<typename T>
T atomic_fetch_add_explicit(atomic<T> *load_store_address,
			    T operand, operation_order success)
{
  return atomic_fetch_op(load_store_address, operand, success,
			  binary_op::add_op);
}

template<typename T>
T atomic_fetch_sub_explicit(atomic<T> *load_store_address,
			    T operand, operation_order success)
{
  return atomic_fetch_op(load_store_address, operand,
			  success, binary_op::sub_op);
}

void atomic_thread_fence(operation_order fence_order)
{
  atomic_fence(fence_order);
}
