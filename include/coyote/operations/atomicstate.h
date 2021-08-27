#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "../scheduler.h"

//@brief set to view debug statements for model functions
//#define ATOMIC_DEBUG_MODEL

//@brief start of the global sequence number
#define START_SEQ_NO 0

// TODO: Preventing vector resize
#define MAX_THREADS 10


class GlobalState;
class ThreadState;
class Atomic_Operation;
class Store;
class Load;
class RMW;
class FetchOp;
class Fence;
class ClockVector;

typedef std::uint64_t value; //@brief value stored at the address
typedef void * location; //@brief the address of the variable
typedef std::uint16_t sequence_number; //@brief global sequence number
typedef std::uint64_t thread_id; //@brief thread id performing atomic operations
typedef std::memory_order memory_order; //@brief memory order tag for the atomic operation
typedef std::uint16_t logical_clock; //@brief logical clock for the vector clocks
typedef std::unordered_map<ThreadState*, std::vector<Atomic_Operation*>> \
thread_operation_list; //@brief operations performed by threads
typedef std::unordered_map<ThreadState*, Atomic_Operation*> \
thread_operation;//@brief an operation performed by threads
typedef std::unordered_map<location, thread_operation_list> obj_thread_operation_map;

enum operation_type {load, store, rmw, fence};
enum operation_order {relaxed, consume, acquire, release, acq_rel, seq_cst};
enum binary_op {add_op, sub_op, and_op, or_op, xor_op};

void initialise_global_state(coyote::Scheduler *);
void reinitialise_global_state();

/**
 * @brief models the thread
 *
 *
 **/
class ThreadState
{
private:
  thread_id thrd_id;

public:
  ThreadState(thread_id);
  
  thread_id get_id()
  {
    return thrd_id;
  }

};



/**
 * @brief Base class for all the atomic operations
 *
 * 
 **/
class Atomic_Operation
{
 protected:
  ThreadState *thread_state;
  location loc;
  operation_type oper_type;
  operation_order oper_order;
  sequence_number seq_no;
  bool seq_cst;
  ClockVector *cv;

 public:
  Atomic_Operation(location, operation_order, operation_type);  

  void set_sequence_number(sequence_number number)
  {
    this->seq_no = number;
  }

  void set_thread(ThreadState *thrd_state)
  {
    thread_state = thrd_state;
  }

  void get_cv();
  
  sequence_number get_sequence_number()
  {
    return seq_no;
  }

  operation_order get_operation_order()
  {
    return oper_order;
  }

  operation_type get_operation_type()
  {
    return oper_type;
  }

  location get_operation_location()
  {
    return loc;
  }
  
  ThreadState* get_thread()
  {
    return thread_state;
  }
  
  bool is_seq_cst()
  {
    seq_cst = (oper_order == operation_order::seq_cst) ? true:false;
    return seq_cst;
  }

  bool happens_before(Atomic_Operation*);
  
  void create_cv();
};



/**
 * @brief ClockVector to happens before relation across atomic operations
 *
 *
 **/
class ClockVector
{
public:
  ClockVector(Atomic_Operation*);
  ~ClockVector();
  bool merge(ClockVector*);
  bool minmerge(ClockVector*);
  bool synchronized_since(Atomic_Operation*);
  logical_clock getClock(thread_id);
  
private:
  //@brief array of logical clocks
  std::vector<logical_clock> clock_vector;

  /** @brief The number of threads recorded in clock (i.e., its length).  */
  std::uint16_t num_threads;
};



/**
 * @brief atomic load
 *
 *
 **/
class Load : Atomic_Operation
{
protected:
  bool is_acquire;
  std::vector<Atomic_Operation*> rf_set;
  ClockVector *rf_cv;
  value load_value;

public:
  Load(location, operation_order, thread_id);
  void build_rf_set();
  std::vector<Atomic_Operation*>& get_rf_set();
  void* load_latest();
  void create_rf_cv();
  Atomic_Operation* choose_random(std::vector<Atomic_Operation*>&);
  void execute();
  void print_rf_set();
  value get_load_value()
  {
    return load_value;
  }
};



/**
 * @brief Atomic store
 *
 *
 **/
class Store : Atomic_Operation
{
protected:
  bool is_release;
  value store_value;

public:
  Store(location, value, operation_order, thread_id);
  void execute();
  value get_value()
  {
    return store_value;
  }
};



/**
 * @brief Atomic CAS operation
 *
 *
 **/
class RMW : Atomic_Operation
{
protected:
  bool return_value;
  bool is_rmw_store;
  operation_order success_mo;
  operation_order failure_mo;
  value expected;
  value desired;
  
public:
  RMW(location, value, operation_order, value, operation_order, thread_id);
  void execute();
  bool get_return_value();
  bool is_store()
  {
    return is_rmw_store;
  }
};



/**
 * @brief Atomic binary operations
 *
 *
 **/
class FetchOp : Atomic_Operation
{
protected:
  binary_op bop;
  bool is_rmw_store;
  value return_value;
  value operand;
  
public:
  FetchOp(location, value, operation_order, thread_id, binary_op);
  void execute();
  value get_return_value();
};

  

/**
 * @brief Atomic Fence operations
 *
 *
 **/
class Fence : Atomic_Operation
{
protected:
  
public:
  Fence(operation_order, thread_id);
  void execute();
};



/**
 * @brief Models atomic operations for an execution trace
 *
 *
 **/
class GlobalState
{
private:
  sequence_number seq_no;

  coyote::Scheduler *scheduler;
  
  //@brief mapping the thread id with internal id
  std::unordered_map<thread_id, thread_id> thread_id_map;
  //@brief mapping the internal thread id with ThreadState
  std::unordered_map<thread_id, ThreadState*> thread_id_obj_map;
  //@brief all the operations performaed at a location
  std::unordered_map<location, std::vector<Atomic_Operation*>> obj_str_map;
  //@brief all operations
  obj_thread_operation_map obj_thread_oper_map;
  //@brief list of store actions by the thread at a location
  obj_thread_operation_map obj_thread_str_map; 
  //@brief list of load actions by the thread at a location
  obj_thread_operation_map obj_thread_ld_map;
  //@brief list of load actions by the thread at a location
  obj_thread_operation_map obj_thread_rmw_map;
  //@brief last thread operation
  std::unordered_map<location, thread_operation> obj_thread_last_oper_map;
  //@brief the latest sequential store action
  std::unordered_map<location, Atomic_Operation*> obj_last_seq_map;

  void record_operation(Atomic_Operation*);
  void record_atomic_store(Atomic_Operation*);
  void record_atomic_load(Atomic_Operation*);
  void record_atomic_rmw(Atomic_Operation*);
  void record_atomic_fence(Atomic_Operation*);

  void insert_into_map();
  void insert_into_map_map();

public:
  GlobalState(sequence_number, coyote::Scheduler*);

  void initialise_sequence_number(sequence_number global_seq_number)
  {
    seq_no = global_seq_number;
  }
  
  sequence_number get_sequence_number();

  thread_id get_thread_id(thread_id tid)
  {
    return thread_id_map.at(tid);
  }

  int get_next_integer(int max)
  {
    return scheduler->next_integer(max);
  }
  
  std::int16_t get_num_of_threads();
  Atomic_Operation* get_last_store(location);
  Atomic_Operation* get_last_seq_cst_store(location);
  void record_atomic_operation(Atomic_Operation*);
  void record_thread(Atomic_Operation*, thread_id);
  void record_modification_order(Atomic_Operation*);

  thread_operation_list get_thread_stores(location loc)
  {
    return obj_thread_str_map.at(loc);
  }
  void print_modification_order();
  void print_thread_stores();
  void print_thread_map();

  void clear_state();
};

extern GlobalState *global_state;
