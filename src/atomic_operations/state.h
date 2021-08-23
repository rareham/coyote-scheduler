#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <vector>


//@brief set to view debug statements for model functions
#define ATOMIC_DEBUG_MODEL 0

//@brief start of the global sequence number
#define START_SEQ_NO 0

// TODO: Preventing vector resize
#define MAX_THREADS 10

typedef std::uint64_t value; //@brief value stored at the address
typedef std::uint64_t location; //@brief the address of the variable
typedef std::uint16_t sequence_number; //@brief global sequence number
typedef std::uint64_t thread_id; //@brief thread id performing atomic operations
typedef std::memory_order memory_order; //@brief memory order tag for the atomic operation
typedef std::uint16_t logical_clock; //@brief logical clock for the vector clocks

enum operation_type {load, store, rmw, fence};
enum operation_order {relaxed, consume, acquire, release, acq_rel, seq_cst};
enum binary_op {add_op, sub_op, and_op, or_op, xor_op};

void initialise_global_state();


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
class Operation
{
 protected:
  ThreadState *thread_state;
  location loc;
  operation_type oper_type;
  operation_order oper_order;
  sequence_number seq_no;
  bool seq_cst;

 public:
  Operation(location, memory_order, operation_type);  

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

  bool is_seq_cst() {
    return seq_cst;
  }
};



/**
 * @brief ClockVector to happens before relation across atomic operations
 *
 *
 **/
class ClockVector
{
public:
  ClockVector(Operation *act = NULL);
  ~ClockVector();
  bool merge(ClockVector *cv);
  bool minmerge(ClockVector *cv);
  bool synchronized_since(Operation *act);
  void print();
  logical_clock getClock(thread_id thread);
  
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
class Load : Operation
{
protected:
  bool is_acquire;
  std::vector<Operation*> rf_set;
  ClockVector *cv;

public:
  Load(value, memory_order, thread_id);
  void build_rf_set();
  std::vector<value>* get_rf_set();
  void* load_latest();
  void execute();
  void create_cv();
};



/**
 * @brief Atomic store
 *
 *
 **/
class Store : Operation
{
protected:
  bool is_release;
  value store_value;
  ClockVector *cv;

public:
  Store(location, value, memory_order, thread_id);
  void execute();
  void create_cv();
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
class RMW : Operation
{
protected:
  memory_order success_mo;
  memory_order failure_mo;
  value expected;
  value desired;
  bool return_value;
  ClockVector *cv;
  
public:
  RMW(location, value, memory_order, value, memory_order, thread_id);
  void execute();
  bool get_return_value();
  void create_cv();
};



/**
 * @brief Atomic binary operations
 *
 *
 **/
class FetchOp : Operation
{
protected:
  memory_order success_mo;
  value return_value;
  value operand;
  ClockVector *cv;
  binary_op bop;
  
public:
  FetchOp(location, value, memory_order, thread_id, binary_op);
  void execute();
  value get_return_value();
  void create_cv();
};

  

/**
 * @brief Atomic Fence operations
 *
 *
 **/
class Fence : Operation
{
protected:
  ClockVector *cv;
  
public:
  Fence(memory_order, thread_id);
  void execute();
};



//@brief operations performed by threads
typedef std::unordered_map<ThreadState*, std::vector<Operation*>> thread_operation_list;
//@brief an operation performed by threads
typedef std::unordered_map<ThreadState*, Operation*> thread_operation;



/**
 * @brief Models atomic operations for an execution trace
 *
 *
 **/
class GlobalState
{
private:
  sequence_number seq_no;

  //@brief mapping the thread id with internal id
  std::unordered_map<thread_id, thread_id> thread_id_map;
  //@brief mapping the internal thread id with ThreadState
  std::unordered_map<thread_id, ThreadState*> thread_id_obj_map;
  //@brief all the operations performaed at a location
  std::unordered_map<location, std::vector<Operation*>> loc_operations;
  //@brief all operations
  std::unordered_map<location, thread_operation_list> obj_oper_thrd_map;
  //@brief list of store actions by the thread at a location
  std::unordered_map<location, thread_operation_list> obj_str_thread_map; 
  //@brief list of load actions by the thread at a location
  std::unordered_map<location, thread_operation_list> obj_ld_thread_map;
  //@brief list of load actions by the thread at a location
  std::unordered_map<location, thread_operation_list> obj_rmw_thread_map;
  //@brief last thread operation
  std::unordered_map<location, thread_operation> obj_last_oper_thread_map;
  //@brief the latest sequential store action
  std::unordered_map<location, Operation*> obj_last_seq_thread_map;

  void record_operation(Operation *);
  void record_atomic_store(Operation *);
  void record_atomic_load(Operation *);
  void record_atomic_rmw(Operation *);
  void record_atomic_fence(Operation *);

  void insert_into_map();
  void insert_into_map_map();

public:
  GlobalState(sequence_number); 
  sequence_number get_sequence_number();
  void add_to_map(std::unordered_map<location, thread_operation_list>*, Operation*);
  thread_id get_thread_id(thread_id tid) {
    return thread_id_map.at(tid);
  }

  void record_atomic_operation(Operation *);
  void record_thread(Operation*, thread_id);
  std::int16_t get_num_of_threads();
  Operation* get_last_store(location);
  Operation* get_last_seq_cst(location);
};

extern GlobalState *global_state;
