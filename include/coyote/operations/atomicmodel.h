/**
 * @brief dummy class to represent atomic variables 
 *
 * The idea is to model the atomic operations and call the 
 * the modeled functions for the atomic operations
 * Replacing the atomic operations on scale is not feasible hence
 * this class. This class does not model the atomic varibles actaully.
 *
 * The modeled funtions dont need the actual atomic variables to
 * model operations. The modeling doesnt affect the DRFness property
 * of the program.
 *
 * Reasons it is fine:
 *
 * Scheduler invariant that only one operation is executing at a time
 * and the interleaving points are only the schedule.next statements.
 * 
 * Also the introduced modeled functions do affect the
 * compiler reordering already hence this class doesnt spill
 * the spilled milk.
 *
 * missing : lock free check
 * Represents either primitives or pointers for atomics variables
 * 
 **/

enum operation_order {relaxed, consume, acquire, release, acq_rel, seq_cst};

template< typename T >
class atomic
{
  public:
  
  atomic() = default;

  constexpr explicit atomic( T store_value ) {
    atomic_store(this, store_value, operation_order::relaxed);
  }
  
  atomic( const atomic& ) = delete;
  atomic& operator =( const atomic& ) = delete;

  void store( T store_value,  operation_order store_order ) ;

  T load( operation_order oper_order ) ;
  
  T exchange( T exchange, operation_order oper_order) ;
  
  bool compare_exchange_weak(T expected, T desired,
			     operation_order success_order,
			     operation_order failure_order ) ;
  
  bool compare_exchange_strong(T expected, T desired,
			       operation_order success_order,
			       operation_order failure_order) ;
  
  // TODO: add decls to capture pointer addition
  T fetch_add(T operand, operation_order oper_order) ;
  
  T fetch_sub(T operand, operation_order oper_order) ;
  
  T fetch_and(T operand, operation_order oper_order) ;
  
  T fetch_or(T operand, operation_order oper_order) ;
  
  T fetch_xor(T operand, operation_order oper_order) ;

  T operator =( T str_value ) ;

  T operator ++() ;

  T operator --() ;
  
  T operator +=( T operand) ;
  
  T operator -=( T operand) ;
};

template<typename T>
T atomic_fetch_add_explicit(atomic<T>* load_store_address,
			      T operand, operation_order success);

template<typename T>
T atomic_fetch_sub_explicit(atomic<T>* load_store_address,
			      T operand, operation_order success);
