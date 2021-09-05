//#include <atomic>
#include <iostream>
#include <cstdint>


#ifndef STATE
#define STATE
#include "atomicstate.h"
#endif

/**
 * // TODO: Reconsider memory order for the operation
 *
 **/
template<typename T>
void atomic_init(atomic<T>* store_address, T init_value)
{
  thread_id tid = global_state->get_scheduled_op_id();

  //typecaste to pointer integer
  location loc = (uintptr_t) store_address;
  Store<T> *s = new Store<T>(loc, init_value, operation_order::relaxed , tid);
  s->execute();

  #ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_init :: initialised at location : " << store_address
	    << " with value : " << init_value << " by thread : "
	    << tid << std::endl;
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
template<typename T>
T atomic_load(atomic<T>* load_address, operation_order load_order)
{
  #ifdef ATOMIC_DEBUG_MODEL
  std::cout << "Started Loading ..." << std::endl;
  #endif

  //assert((load_address != nullptr) && "atomic_load : Load address is NULL");
  
  thread_id tid = global_state->get_scheduled_op_id();
  location loc = (uintptr_t) load_address;
  Load<T> *l = new Load<T>(loc, load_order, tid);
  T ret_val = l->execute();
    #ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_load :: load from location : " << load_address
	    << " with order : " << load_order
	    << " by thread : " << tid << std::endl;
#endif

  return ret_val;
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
template<typename T>
void atomic_store(atomic<T>* store_address, T store_value, 
		  operation_order store_order)
{
#ifdef ATOMIC_DEBUG_MODEL
  std::cout << "Storing ..." << std::endl;
#endif

  //assert((store_address != nullptr) && "atomic_store : Store address is NULL");

  thread_id tid = global_state->get_scheduled_op_id();
  location loc = (uintptr_t) store_address;
  
  Store<T> *s = new Store<T>(loc, store_value, store_order, tid);
  s->execute();

#ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_store :: stored at location : " << store_address
	    << " with value : " << store_value
	    << " store_order : " << store_order
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
template<typename T>
bool atomic_rmw(atomic<T>* load_store_address,T expected, T desired,
		operation_order success_order ,
		operation_order failure_order )
{

#ifdef ATOMIC_DEBUG_MODEL
  std::cout << "RMWing ..." << std::endl;
#endif
  
  thread_id tid = global_state->get_scheduled_op_id();

  location loc = (uintptr_t) load_store_address;  

  RMW<T> *rmw = new RMW<T>(loc, expected, success_order, desired,
			   failure_order, tid);
  rmw->execute();
    

#ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_rmw :: rmw at location : " << load_store_address
	    << "with expected_value : " << expected
	    << " success_order : " << success_order
	    << " with desired_value : " << desired
	    << " failure_order : " << failure_order
	    << " by thread : " << tid << std::endl;
#endif

  bool ret_val = rmw->get_return_value();

  return ret_val;
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
template<typename T>
T atomic_fetch_add(atomic<T>* load_store_address, value operand,
		   operation_order memory_order)
{
#ifdef ATOMIC_DEBUG_MODEL
  std::cout << "FOP adding ..." << std::endl;
#endif

  thread_id tid = global_state->get_scheduled_op_id();
  
  location loc = (uintptr_t) load_store_address;
  
  FetchOp<T> *fop = new FetchOp<T>(loc, operand, memory_order,
				   tid, binary_op::add_op);
  fop->execute();

  T ret_val = fop->get_return_value();
  
#ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_fetch_add :: stored at location : " << load_store_address
	    << " with value : " << ret_val << " order : " << memory_order
	    << " by thread : " << tid << std::endl;
#endif

  return ret_val; 
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
template<typename T>
T atomic_fetch_sub(atomic<T>* load_store_address, value operand,
		   operation_order memory_order)
{
#ifdef ATOMIC_DEBUG_MODEL
  std::cout << "FOP subing ..." << std::endl;
#endif

  thread_id tid = global_state->get_scheduled_op_id();
  location loc = (uintptr_t) load_store_address;
  
  FetchOp<T> *fop = new FetchOp<T>(loc, operand, memory_order,
				   tid, binary_op::sub_op);
  fop->execute();

  T ret_val = fop->get_return_value();
  
#ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_fetch_sub :: stored at location : " << load_store_address
	    << " with value : " << ret_val << " order : " << memory_order
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
template<typename T>
T atomic_fetch_and(atomic<T>* load_store_address, value operand,
		   operation_order memory_order)
{
#ifdef ATOMIC_DEBUG_MODEL
  std::cout << "FOP anding ..." << std::endl;
#endif

  thread_id tid = global_state->get_scheduled_op_id();
  location loc = (uintptr_t) load_store_address;
  
  FetchOp<T> *fop = new FetchOp<T>(loc, operand, memory_order,
				   tid, binary_op::and_op);
  fop->execute();

  T ret_val = fop->get_return_value();

#ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_fetch_and :: stored at location : " << load_store_address
	    << " with value : " << ret_val << " order : " << memory_order
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
template<typename T>
T atomic_fetch_or(atomic<T>* load_store_address, value operand, 
		  operation_order memory_order )
{
#ifdef ATOMIC_DEBUG_MODEL
  std::cout << "FOP oring ..." << std::endl;
#endif

  thread_id tid = global_state->get_scheduled_op_id();

  location loc = (uintptr_t) load_store_address;

  FetchOp<T> *fop = new FetchOp<T>(loc, operand, memory_order,
				   tid, binary_op::or_op);

  fop->execute();

  T ret_val = fop->get_return_value();
    
#ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_fetch_or :: stored at location : " << load_store_address
	    << " with value : " << ret_val << " order : " << memory_order
	    << " by thread : " << tid << std::endl;
#endif

  return fop->get_return_value();

}



template<typename T>
T atomic_exchange(atomic<T>* load_store_address, T exchange,
		  operation_order memory_order)
{
#ifdef ATOMIC_DEBUG_MODEL
  std::cout << "Exchanging ..." << std::endl;
#endif

  thread_id tid = global_state->get_scheduled_op_id();

  location loc = (uintptr_t) load_store_address;

  Xchange<T> *xchng = new Xchange<T>(loc, memory_order, exchange, tid);
  
  T ret_val = xchng->get_return_value();
  
  return ret_val;
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
template<typename T>
T atomic_fetch_xor(atomic<T>* load_store_address, value operand,
		   operation_order memory_order )
{
#ifdef ATOMIC_DEBUG_MODEL
  std::cout << "FOP xoring ..." << std::endl;
#endif

  thread_id tid = global_state->get_scheduled_op_id();

  location loc = (uintptr_t) load_store_address;
  
  FetchOp<T> *fop = new FetchOp<T>(loc, operand, memory_order,
				   tid, binary_op::xor_op);
  fop->execute();

  T ret_val = fop->get_return_value();
    
#ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_fetch_xor :: stored at location : " << load_store_address
	    << " with value : " << ret_val << " order : " << memory_order
	    << " by thread : " << tid << std::endl;
#endif

  return fop->get_return_value();
}



/**
 *
 * @brief models atomic fence
 *
 * void atomic_thread_fence( std::memory_order order );
 **/
void atomic_fence(operation_order fence_order )
{
#ifdef ATOMIC_DEBUG_MODEL
  std::cout << "Fencing...." << std::endl;
#endif

  thread_id tid = global_state->get_scheduled_op_id();

  Fence *fence = new Fence(fence_order, tid);
  fence->execute();
#ifdef ATOMIC_DEBUG_MODEL
  std::cout << "atomic_fence :: order : " << fence_order
	    << " by thread : " << tid << std::endl;
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
T atomic<T>::fetch_add(T operand, operation_order oper_order)
{
  return atomic_fetch_add(this, operand, oper_order);
}

template<typename T>
T atomic<T>::fetch_sub(T operand, operation_order oper_order)
{
  return atomic_fetch_sub(this, operand, oper_order);
}

template<typename T>
T atomic<T>::fetch_and(T operand, operation_order oper_order)
{
  return atomic_fetch_and(this, operand, oper_order);
}

template<typename T>
T atomic<T>::fetch_or(T operand, operation_order oper_order)
{
  return atomic_fetch_or(this, operand, oper_order);
}

template<typename T>
T atomic<T>::fetch_xor(T operand, operation_order oper_order)
{
  return atomic_fetch_xor(this, operand, oper_order);
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
  return atomic_fetch_add(this, 1, operation_order::seq_cst);
}

template<typename T>
T atomic<T>::operator --()
{
  return atomic_fetch_sub(this, 1, operation_order::seq_cst);
}

template<typename T>
T atomic<T>::operator +=( T operand) 
{
  return atomic_fetch_add(this, operand, operation_order::seq_cst);
}

template<typename T>
T atomic<T>::operator -=( T operand) 
{
  return atomic_fetch_sub(this, operand, operation_order::seq_cst);
}

template<typename T>
T atomic_fetch_add_explicit(atomic<T>* load_store_address,
			    T operand, operation_order success)
{
  return atomic_fetch_add(load_store_address, operand,
			  operation_order::relaxed);
}

template<typename T>
T atomic_fetch_sub_explicit(atomic<T>* load_store_address,
			    T operand, operation_order success)
{
  return atomic_fetch_sub(load_store_address, operand,
			  operation_order::relaxed);
}
