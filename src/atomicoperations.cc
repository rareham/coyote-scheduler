#include <cassert>
#include <iostream>
#include <ostream>
#include <vector>
#include <cstdint>

#include "operations/atomicoperations.h"

#ifndef STATE
#define STATE
#include "operations/atomicstate.h"
#endif


//@brief stores state for atomic operations in an execution
GlobalState *global_state = nullptr;


/**
 * // TODO: Reconsider memory order for the operation
 *
 **/
void atomic_init(thread_id tid, value init_value = 0, location store_address = nullptr)
{
  assert(store_address && "atomic_init :: store_address is NULL");

  Store *s = new Store(store_address, init_value, operation_order::seq_cst , tid);
  s->execute();

  #ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_init :: initialised at location : " << store_address
	    << " with value : " << init_value << " by thread : " << tid << std::endl;
  #endif
}



/**
 * @brief models atomic load operations
 *
 * T atomic_load( const std::atomic<T>* );
 *
 * T atomic_load_explicit( const std::atomic<T>* ,
 * std::memory_order ) ;
 **/
value atomic_load(thread_id tid, location load_address = nullptr,
		  operation_order load_order = operation_order::seq_cst)
{
  #ifdef ATOMIC_DEBUG_MODEL
  std::cout << "Started Loading ..." << std::endl;
  #endif

  Load *l = new Load(load_address, load_order, tid);
  l->execute();

  #ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_load :: load from location : " << load_address
	    << " with order : " << load_order << " by thread : " << tid << std::endl;
  #endif
  return l->get_load_value();
}



/**
 * @brief models atomic store operation
 *
 * void atomic_store( volatile std::atomic<T>*,
 * typename std::atomic<T>::value_type);
 *
 * void atomic_store_explicit( std::atomic<T>* ,
 * typename std::atomic<T>::value_type ,std::memory_order )
 *
 **/
void atomic_store(thread_id tid, value store_value = 0, location store_address = nullptr,
		  operation_order store_order = operation_order::seq_cst)
{
  #ifdef ATOMIC_DEBUG_MODEL
  std::cout << "Storing ..." << std::endl;
  #endif

  Store *s = new Store(store_address, store_value, store_order, tid);
  s->execute();

  #ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_store :: stored at location : " << store_address
	    << " with value : " << store_value << " store_order : " << store_order
	    << " by thread : " << tid << std::endl;
  #endif

  global_state->print_thread_stores();

}



/**
 * @brief models atomic rmw operations
 *
 * bool atomic_compare_exchange_weak( std::atomic<T>* obj,
 * typename std::atomic<T>::value_type* expected,
 * typename std::atomic<T>::value_type desired ) noexcept;
 *
 * bool atomic_compare_exchange_strong( std::atomic<T>* obj,
 * typename std::atomic<T>::value_type* expected,
 * typename std::atomic<T>::value_type desired ) ;
 *
 * bool atomic_compare_exchange_weak_explicit( std::atomic<T>* obj,
 * typename std::atomic<T>::value_type* expected,
 * typename std::atomic<T>::value_type desired,
 * std::memory_order succ,
 * std::memory_order fail );
 *
 * bool atomic_compare_exchange_strong_explicit( std::atomic<T>* obj,
 * typename std::atomic<T>::value_type* expected,
 * typename std::atomic<T>::value_type desired,
 * std::memory_order succ,
 * std::memory_order fail ) ;
 *
 **/
value atomic_rmw(thread_id tid, location load_store_address,
		 value expected, value desired,
		 operation_order success_order = operation_order::seq_cst, \
		 operation_order failure_order = operation_order::seq_cst)
{
    
  RMW *rmw = new RMW(load_store_address, expected, success_order, desired, \
		     failure_order, tid);
  rmw->execute();

  #ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_rmw :: rmw at location : " << load_store_address
	    << "with expected_value : " << expected
	    << " success_order : " << success_order
	    << " with desired_value : " << desired << " failure_order : " << failure_order
	    << " by thread : " << tid << std::endl;
  #endif

  return rmw->get_return_value();
}



/**
 * @brief models atomic fetch and binary operation(rmw)
 *
 * atomic_fetch_add( std::atomic<T>* obj,
 * typename std::atomic<T>::difference_type arg ) ;
 *
 * 
 * T atomic_fetch_add_explicit( std::atomic<T>* obj,
 * typename std::atomic<T>::difference_type arg,
 * std::memory_order order ) ;
 *
 **/
value atomic_fetch_add(thread_id tid, value operand,
		       location load_store_address = nullptr,
		       operation_order memory_order = operation_order::seq_cst)
{
  
  FetchOp *fop = new FetchOp(load_store_address, operand, memory_order,
			     tid, binary_op::add_op);
  fop->execute();

  #ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_fetch_add :: stored at location : " << load_store_address
	    << " with value : " << operand << " order : " << memory_order
	    << " by thread : " << tid << std::endl;
  #endif

  return fop->get_return_value();
}



/**
 * @brief models atomic fetch and binary operation(rmw)
 *
 * T atomic_fetch_sub( std::atomic<T>* obj,
 * typename std::atomic<T>::difference_type arg ) ;
 *
 * T atomic_fetch_sub_explicit( std::atomic<T>* obj,
 * typename std::atomic<T>::difference_type arg,
 * std::memory_order order );
 * 
 **/
value atomic_fetch_sub(thread_id tid, value operand,
		       location load_store_address = nullptr,
		       operation_order memory_order = operation_order::seq_cst)
{
  FetchOp *fop = new FetchOp(load_store_address, operand, memory_order,
			     tid, binary_op::sub_op);
  fop->execute();

  #ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_fetch_sub :: stored at location : " << load_store_address
	    << " with value : " << operand << " order : " << memory_order
	    << " by thread : " << tid << std::endl;
  #endif

  return fop->get_return_value();

}



/**
 * @brief models atomic fetch and binary operation(rmw)
 *
 * T atomic_fetch_and( std::atomic<T>* obj,
 * typename std::atomic<T>::value_type arg ) noexcept;
 *
 * T atomic_fetch_and_explicit( std::atomic<T>* obj,
 * typename std::atomic<T>::value_type arg,
 * std::memory_order order) noexcept;
 **/
value atomic_fetch_and(thread_id tid, value operand,
		       location load_store_address = nullptr,
		       operation_order memory_order = operation_order::seq_cst)
{
  FetchOp *fop = new FetchOp(load_store_address, operand, memory_order,
			     tid, binary_op::and_op);
  fop->execute();

  #ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_fetch_and :: stored at location : " << load_store_address
	    << " with value : " << operand << " order : " << memory_order
	    << " by thread : " << tid << std::endl;
  #endif

  return fop->get_return_value();
}



/**
 * @brief models atomic fetch and binary operation(rmw)
 *
 * T atomic_fetch_or( std::atomic<T>* obj,
 * typename std::atomic<T>::value_type arg ) noexcept;
 *
 * T atomic_fetch_or_explicit( std::atomic<T>* obj,
 * typename std::atomic<T>::value_type arg,
 * std::memory_order order) noexcept;
 **/
value atomic_fetch_or(thread_id tid, value operand, location load_store_address = nullptr,
		      operation_order memory_order = operation_order::seq_cst)
{
  FetchOp *fop = new FetchOp(load_store_address, operand, memory_order,
			     tid, binary_op::or_op);
  fop->execute();
  #ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_fetch_or :: stored at location : " << load_store_address
	    << " with value : " << operand << " order : " << memory_order
	    << " by thread : " << tid << std::endl;
  #endif

  return fop->get_return_value();

}



/**
 * @brief models atomic fetch and binary operation(rmw)
 *
 * T atomic_fetch_xor( std::atomic<T>* obj,
 * typename std::atomic<T>::value_type arg ) noexcept;
 *
 * T atomic_fetch_xor_explicit( std::atomic<T>* obj,
 * typename std::atomic<T>::value_type arg,
 * std::memory_order order) noexcept;
 *
 **/
value atomic_fetch_xor(thread_id tid, value operand,
		       location load_store_address = nullptr,
		       operation_order memory_order = operation_order::seq_cst)
{
  FetchOp *fop = new FetchOp(load_store_address, operand, memory_order,
			     tid, binary_op::xor_op);
  fop->execute();

  #ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_fetch_xor :: stored at location : " << load_store_address
	    << " with value : " << operand << " order : " << memory_order
	    << " by thread : " << tid << std::endl;
  #endif

  return fop->get_return_value();
}



/**
 *
 * @brief models atomic fence
 *
 * void atomic_thread_fence( std::memory_order order ) ;
 **/
void atomic_fence(thread_id tid, operation_order fence_order = operation_order::seq_cst)
{
  Fence *fence = new Fence(fence_order, tid);
  fence->execute();
  #ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_fence :: order : " << fence_order
	    << " by thread : " << tid << std::endl;
  #endif
}
