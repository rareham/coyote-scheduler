/**
 * @brief dummy class to represent atomic variables 
 *
 * The idea is to model the atomic operations and call the 
 * the modeled functions for the atomic operations
 * Replacing the atomic operations on scale is not feasible hence
 * this class. This class does not model the atomic varibles
 * in complete.
 *
 * Reasons it is fine:
 *
 * Scheduler invariant that only one operation is executing at a time
 * and the interleaving points are only the schedule.next statements.
 * 
 **/

#pragma once

#include <iostream>
#include <ostream>
namespace std
{
  typedef enum operation_order
    {
     relaxed, consume, acquire,
     release, acq_rel, seq_cst
    } operation_order;

}

// to replace the std::memory_order_* usages
typedef std::operation_order operation_order;

operation_order relaxed = operation_order::relaxed;
operation_order consume = operation_order::consume;
operation_order acquire = operation_order::acquire;
operation_order release = operation_order::release;
operation_order acq_rel = operation_order::acq_rel;
operation_order seq_cst = operation_order::seq_cst;

  
#define memory_order_seq_cst seq_cst
#define memory_order_acquire acquire
#define memory_order_release release
#define memory_order_consume consume
#define memory_order_acq_rel acq_rel
#define memory_order_relaxed relaxed

#define memory_order operation_order

template< typename T >
class atomic

{
public:
  atomic()
  {}
  
  explicit atomic( T store_value )
  {
    atomic_store(this, store_value, memory_order_relaxed);
  }
  
  atomic( const atomic& ) = delete;
  atomic& operator =( const atomic& ) = delete;

  void store( T store_value,  std::operation_order store_order ) ;

  T load( std::operation_order oper_order ) ;
  
  T exchange( T exchange, std::operation_order oper_order) ;

  T exchange( T exchange );
  
  bool compare_exchange_weak( T expected, T desired,
			      std::operation_order success_order,
			     std::operation_order failure_order ) ;
  
  bool compare_exchange_strong( T expected, T desired,
				std::operation_order success_order,
			       std::operation_order failure_order ) ;


  bool compare_exchange_strong( T expected, T desired,
				std::operation_order success_order ) ;
  
  T fetch_add( T operand, std::operation_order oper_order ) ;
  
  T fetch_sub( T operand, std::operation_order oper_order ) ;
  
  T fetch_and( T operand, std::operation_order oper_order ) ;
  
  T fetch_or( T operand, std::operation_order oper_order ) ;
  
  T fetch_xor( T operand, std::operation_order oper_order ) ;

  T operator =( T str_value ) ;

  T operator ++() ;

  T operator --() ;
  
  T operator +=( T operand ) ;
  
  T operator -=( T operand ) ;
};

template<typename T>
T atomic_fetch_add_explicit( atomic<T>* load_store_address,
			     T operand, std::operation_order success );

template<typename T>
T atomic_fetch_sub_explicit( atomic<T>* load_store_address,
			     T operand, std::operation_order success );
