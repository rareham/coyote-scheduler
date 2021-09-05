//#include <atomic>
#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "../scheduler.h"
#include "atomicmodel.h"

//@brief set to view debug statements for model functions
//#define ATOMIC_DEBUG_MODEL

//@brief start of the global sequence number
#define START_SEQ_NO 0

// TODO: Preventing vector resize
#define MAX_THREADS 20


class GlobalState;
class ThreadState;
class Atomic_Operation;
template<typename T> class Store;
template<typename T> class Load;
template<typename T> class RMW;
template<typename T> class Xchange;
template<typename T> class FetchOp;
class Fence;
class ClockVector;

typedef std::uint64_t value; //@brief value stored at the address
typedef std::uintptr_t location; //@brief the address of the variable
typedef std::uint32_t sequence_number; //@brief global sequence number
typedef std::uint64_t thread_id; //@brief thread id performing atomic operations
//@brief memory order tag for the atomic operation
typedef std::memory_order memory_order;
//@brief logical clock for the vector clocks
typedef std::uint32_t logical_clock;
typedef std::unordered_map<ThreadState*, std::vector<Atomic_Operation*>> \
thread_operation_list; //@brief operations performed by threads
typedef std::vector<Atomic_Operation*> operation_list;
typedef std::unordered_map<ThreadState*, Atomic_Operation*> \
thread_operation;//@brief an operation performed by threads
typedef std::unordered_map<location, thread_operation_list> \
obj_thread_operation_map;

enum operation_type {load, store, rmw, fop, rmwr, xchange, fence};
enum binary_op {add_op, sub_op, and_op, or_op, xor_op};

void initialise_global_state(coyote::Scheduler *);
void reinitialise_global_state();

//@brief stores state for atomic operations in an execution
GlobalState *global_state = nullptr;


/**
 * @brief models the thread
 *
 **/
class ThreadState
{
private:
    thread_id thrd_id;

public:
    ThreadState(thread_id);

    thread_id get_id();

};



/**
 * @brief ClockVector to happens before relation across atomic operations
 *
 *
 **/
class ClockVector
{
public:
  ClockVector(thread_id, sequence_number);
  ~ClockVector();
  bool merge(ClockVector*);
  bool minmerge(ClockVector*);
  bool synchronized_since(thread_id, sequence_number);
  logical_clock getClock(thread_id);
  
private:
  //@brief array of logical clocks
  std::vector<logical_clock> clock_vector;

  /** @brief The number of threads recorded in clock (i.e., its length).  */
  std::uint16_t num_threads;
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
  operation_order oper_order = operation_order::seq_cst;
  //@brief sequence number of the operation
  sequence_number seq_no;
  //@brief true if the operation is seq_cst
  bool seq_cst;
  //@brief clock vector associated with the operation
  ClockVector *cv = nullptr;
  //@brief the reads from cv of the store the loads reads from
  ClockVector *rf_cv = nullptr;
  //@brief the reads from operation for a load and rmw
  Atomic_Operation *rf = nullptr;

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

  ClockVector* get_cv()
  {
    return cv;
  }

  ClockVector* get_rf_cv()
  {
    return rf_cv;
  }
  
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

  Atomic_Operation* get_rf()
  {
    return rf;
  }
  
  bool is_seq_cst()
  {
    seq_cst = (oper_order == operation_order::seq_cst) ? true:false;
    return seq_cst;
  }

  void set_rf_cv(ClockVector *cv)
  {
    rf_cv = cv;
  }

  bool happens_before(Atomic_Operation*);
  
  void create_cv();

  bool is_acquire()
  {
    if (oper_order == operation_order::acquire
	|| oper_order == operation_order::acq_rel
	|| oper_order == operation_order::seq_cst)
      {
	return true;
      }
    return false;
  }

  bool is_release()
  {
    if (oper_order == operation_order::release
	|| oper_order == operation_order::acq_rel
	|| oper_order == operation_order::seq_cst)
      {
	return true;
      }
    return false;
  }

  bool is_store()
  {
    if (oper_type == operation_type::rmw
	|| oper_type == operation_type::store)
      {
	return true;
      }
    return false;
  }

  bool is_rmw()
  {
    if (oper_type == operation_type::rmw)
      {
	return true;
      }
    return false;
  }

  bool is_fop()
  {
    if (oper_type == operation_type::fop)
      {
	return true;
      }
    return false;
  }

  bool is_xchange()
  {
    if (oper_type == operation_type::xchange)
      {
	return true;
      }
    return false;
  }

  ClockVector* get_hb_from_store(Atomic_Operation*);
};


/**
 * @brief atomic load
 *
 *
 **/
template<typename T>
class Load : Atomic_Operation
{
protected:
  ClockVector *rf_cv;

public:
  Load(location, operation_order, thread_id);
  void build_choose_rf();
  void* load_latest();
  bool validate_rf(Atomic_Operation*);
  void create_rf_cv();
  int choose_random(operation_list &);
  void choose_validate(operation_list &);
  T execute();
  void print_rf_set(operation_list &);
  T get_rf_value(Atomic_Operation*);
};



/**
 * @brief Atomic store
 *
 *
 **/
template<class T>
class Store : Atomic_Operation
{
protected:
  T store_value;

public:
  Store(location, T, operation_order, thread_id);
  void execute();
  T get_value()
  {
    //std::cout << "store_value: " << store_value << std::endl;
    return this->store_value;
  }
};



/**
 * @brief Atomic CAS operation
 *
 *
 **/
template<typename T>
class RMW : Atomic_Operation
{
protected:
  T rmw_value;
  bool return_value;
  operation_order success_mo;
  operation_order failure_mo;
  T expected;
  T desired;
  T get_rf_value(Atomic_Operation*);
  
public:
  RMW(location, T, operation_order, T, operation_order, thread_id);
  void execute();
  T get_value();
  bool get_return_value();
};



/**
 * @brief Atomic exchange operation
 *
 *
 **/
template<typename T>
class Xchange : Atomic_Operation
{
protected:
  T xchange_value;
  T return_value;
  
public:
  Xchange(location, operation_order, T, thread_id);
  void execute();
  T get_value();
  T get_return_value();
  T get_rf_value(Atomic_Operation*);
};



/**
 * @brief Atomic binary operations
 *
 *
 **/
template<typename T>
class FetchOp : Atomic_Operation
{
protected:
  Atomic_Operation *rf;
  binary_op bop;
  T fop_val;
  T return_value;
  value operand;
  
public:
  FetchOp(location, value, operation_order, thread_id, binary_op);
  void execute();
  T get_return_value();
  T get_value();
  T get_rf_value(Atomic_Operation*);
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
    std::unordered_map<location, operation_list> obj_str_map;
    //@brief thread and operations map - debugging purpose
    std::unordered_map<ThreadState*, operation_list> thread_oper_map;
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

    //@brief gets the id of the operation scheduled by the thread
    int get_scheduled_op_id()
    {
      return scheduler->scheduled_operation_id();
    }
  
    std::int16_t get_num_of_threads();
    Atomic_Operation* get_last_store(location);
    Atomic_Operation* get_last_seq_cst_store(location);
    void record_atomic_operation(Atomic_Operation*);
    void record_thread(Atomic_Operation*, thread_id);

    thread_operation_list get_thread_stores(location loc)
    {
      return obj_thread_str_map.at(loc);
    }

    thread_operation_list get_thread_opers(location loc)
    {
      return obj_thread_oper_map.at(loc);
    }

    int get_num_threads()
    {
      return thread_id_map.size();
    }

    bool check_stores_in_order(Atomic_Operation *, Atomic_Operation *);
    void print_modification_order();
    void print_thread_stores();
    void print_thread_map();

    void clear_state();
  };



  ThreadState::ThreadState(thread_id thrd_id)
  : thrd_id(thrd_id)
{}



thread_id ThreadState::get_id()
{
  return thrd_id;
}



ClockVector::ClockVector(thread_id tid, sequence_number seq_no)
  : clock_vector(MAX_THREADS, 0)
{
  num_threads = global_state->get_num_of_threads();
  clock_vector[tid] = seq_no;
}



ClockVector::~ClockVector()
{}



bool ClockVector::merge(ClockVector *cv)
{
  //assert((cv != nullptr), ErrorCode::Failure);
  
  bool changed = false;
  
  for (int i = 0;i < cv->num_threads;i++)
    if (cv->clock_vector[i] > clock_vector[i]) {
      clock_vector[i] = cv->clock_vector[i];
      changed = true;
    }

  return changed;
}



bool ClockVector::minmerge(ClockVector *cv)
{
  //assert(cv, ErrorCode::Failure);

  bool changed = false;
  for (int i = 0;i < cv->num_threads;i++)
    if (cv->clock_vector[i] < clock_vector[i]) {
      clock_vector[i] = cv->clock_vector[i];
      changed = true;
    }

  return changed;
}



bool ClockVector::synchronized_since(thread_id tid, sequence_number seq_no)
{
  if (tid < num_threads)
    return seq_no <= clock_vector[tid];
  return false;
}



logical_clock ClockVector::getClock(thread_id tid)
{
  if (tid < num_threads)
    return clock_vector[tid];
  else
    return 0;
}



Atomic_Operation::Atomic_Operation(location address, operation_order oper_order,
				   operation_type oper_type)
  : loc(address), oper_order((operation_order)oper_order), oper_type(oper_type)
{
  if ((operation_order)oper_order == operation_order::seq_cst)
    {
      seq_cst = true;
    }
}



bool Atomic_Operation::happens_before(Atomic_Operation *oper)
{
  return oper->cv->synchronized_since(this->get_thread()->get_id(),
				      this->get_sequence_number());
}



void Atomic_Operation::create_cv()
{
  cv = new ClockVector(this->get_thread()->get_id(),
		       this->get_sequence_number());
}



/**
 * @brief builds the rf clock for the load operation
 *
 * The RF clock is built inductively, note the rf clock
 * is not maintained seperately, only when a load affects the
 * the thread clock vector the rf cv is evaluated on the fly.
 * // TODO: Check the implementation again
 **/

ClockVector* Atomic_Operation::get_hb_from_store(Atomic_Operation *rf)
{
  // rfset of the locations the stores load from, for rmw opers
  operation_list rmw_rf_set;

  // TODO: change the loop
  if (rf->is_rmw())
    {
      Atomic_Operation* reads_from = rf->get_rf();
      rmw_rf_set.push_back(rf);
    }
  
  ClockVector *vec = nullptr;
  
  do {
    if (rf->get_rf_cv() != nullptr)
      {
	vec = rf->get_rf_cv();
      }
    else if (rf->is_acquire() && rf->is_release())
      {
	vec = rf->get_cv();
      }
    else if (rf->is_release() && !rf->is_rmw())
      {
	vec = rf->get_cv();
      }
    else if (rf->is_release())
      {
	vec = new ClockVector(0, 0);
	vec->merge(rf->get_cv());
	rf->set_rf_cv(vec);
      }
    else
      {
	if (vec == nullptr)
	  {
	    if (rf->is_rmw())
	      {
		vec = new ClockVector(0, 0);
	      }
	  }
	else
	  {
	    vec = new ClockVector(0, 0);
	  }
	rf->set_rf_cv(vec);
      }
    if (rmw_rf_set.empty())
      {
        break;
      }
    rf = rmw_rf_set.front();
    rmw_rf_set.pop_back();
  } while (true);
  
  return vec;
}



/**
 *
 * @brief Checks if reading from operation *rf for a load lead to cycles
 * 
 * The modification order here is acyclic chain - not a dag.
 **/
template<class T>
bool Load<T>::validate_rf(Atomic_Operation *rf)
{
  thread_operation_list inner_map = global_state->get_thread_opers(loc);
  thread_operation_list::iterator thread_iter = inner_map.begin();

  while (thread_iter != inner_map.end())
    {
      operation_list opers = thread_iter->second;
      //for each operation by the thread, from latest to earliest
      operation_list::reverse_iterator oper_iter = opers.rbegin();

      while (oper_iter != opers.rend())
	{
	  Atomic_Operation *oper = *oper_iter;
	  
	  if (oper == this)
	    {
	      oper_iter++;
	      continue;
	    }

	  if (oper->happens_before(rf))
	    {
	      // TODO: consider the rf due to rmw operations?
	      if (oper->is_store())
		{
		  if (global_state->check_stores_in_order(oper, rf))
		    return false;
		}
	      else
		{
		  Atomic_Operation *prevrf = ((Load<T>*)oper)->get_rf();
		  if (!(prevrf == rf))
		    {
		      if (global_state->check_stores_in_order(rf, prevrf))
			return false;
		    }
		}
	      break;
	    }
      oper_iter++;
	}
      thread_iter++;
    }
  return true;
}



template<class T>
Load<T>::Load(location load_address, operation_order load_order, thread_id tid)
  : Atomic_Operation(load_address, load_order, operation_type::load)
{
  sequence_number seq_no = global_state->get_sequence_number();
  set_sequence_number(seq_no);
  global_state->record_thread(this, tid);
  create_cv();
}



/**
 * @brief chooses the read from value based on scheduler strategy
 *
 **/
template<typename T>
int Load<T>::choose_random(operation_list &rf_set)
{
  int iter_int = global_state->get_next_integer(rf_set.size());
  return iter_int;
}



/**
 *
 * @brief collects all the stores happening before and happens-before the load
 *
 **/
template<class T>
void Load<T>::build_choose_rf()
{
  operation_list rf_set;
  Atomic_Operation *last_sc_store = nullptr;
  location loc = this->get_operation_location();

  thread_operation_list inner_map = global_state->get_thread_stores(loc);

  if (this->is_seq_cst())
    {
      Atomic_Operation *last_sc_store =
	global_state->get_last_seq_cst_store(loc);
    }

  thread_operation_list::iterator thread_iter = inner_map.begin();
  //for each thread
  while (thread_iter != inner_map.end())
    {
      operation_list store_opers = thread_iter->second;
      //for each operation by the thread, from latest to earliest
      operation_list::reverse_iterator store_iter = store_opers.rbegin(); 
      while (store_iter != store_opers.rend())
	{
	  Atomic_Operation *store_oper = *store_iter;
	  if (store_oper == this)
	    continue;

	  bool allow_load = true;

	  if (this->is_seq_cst() &&
	      (store_oper->is_seq_cst() ||
	       (last_sc_store != nullptr
		&& store_oper->happens_before(last_sc_store))) &&
	      store_oper != last_sc_store)
	    allow_load = false;

	  if (allow_load) {
	    rf_set.push_back(store_oper);
	  }

	  if (store_oper->happens_before(this))
	    break;

	  store_iter++;
	}
      thread_iter++;
    }
  //print the rfset if debug enabled
  this->print_rf_set(rf_set);
  //choose the store from rf set
  this->choose_validate(rf_set);
}



/**
 * @brief choose the reads-from from the reads-from set
 * and validate if the reads-from forms a consistent execution
 **/
template<class T>
void Load<T>::choose_validate(operation_list &rf_set)
{
  //choose a store to read from
  while(true)
    {
      //index of strategically chosen reads-from
      int index = choose_random(rf_set);
      Atomic_Operation *rf_store = rf_set[index];
      //validate the reads-from for the load
      if (this->validate_rf(rf_store))
	{
	  this->rf = rf_store;
	  if (this->is_acquire()) {
	    ClockVector *str_rf_cv = get_hb_from_store(rf_store);
	    if (str_rf_cv == nullptr)
	      {
		return;
	      }
	    this->get_cv()->merge(str_rf_cv);
	  }
	  this->rf_cv = rf_store->get_cv();
	  return;
	}

      rf_set[index] = rf_set.back();
      rf_set.pop_back();
      if (rf_set.empty())
	{
          #ifdef ATOMIC_DEBUG_MODEL
	  std::cout << "Load::build_choose_rf : no valid stores found"
		    << std::endl;
          #endif
	  this->rf = nullptr;
	  return;
	}
    }
}



template<class T>
void Load<T>::create_rf_cv()
{
  this->rf_cv =
    new ClockVector(this->get_thread()->get_id(), this->get_sequence_number());
}



template<class T>
void Load<T>::print_rf_set(operation_list &rf_set)
{
#ifdef ATOMIC_DEBUG_MODEL
  operation_list::iterator rf_set_iter = rf_set.begin();
  while (rf_set_iter != rf_set.end())
    {
      Atomic_Operation *rf_oper = *rf_set_iter;

      // TODO: Check if it can be done better
      T rf_val = this->get_rf_value(rf_oper);
      
      std::cout << "Load::print_rf_set : location "
		<< rf_oper->get_operation_location()
		<< " value = " << rf_val << std::endl;
      rf_set_iter++;
    }
#endif
}


/**
 *
 * 
 **/
template<typename T>
T Load<T>::get_rf_value(Atomic_Operation *rf_oper)
{
  T rf_val;
  if (rf_oper->is_store())
    {
      rf_val = ((Store<T>*)rf_oper)->get_value();
    }
  else if (rf_oper->is_rmw())
    {
      rf_val = ((RMW<T>*)rf_oper)->get_value();
    }
  else if (rf_oper->is_fop())
    {
      rf_val = ((FetchOp<T>*)rf_oper)->get_value();
    }
  else if (rf_oper->is_xchange())
    {
      rf_val = ((Xchange<T>*)rf_oper)->get_value();
    }
  return rf_val;
}



/**
 * @brief records the load operation and returns a store based on the 
 * strategy of the scheduler
 *
 **/
template<typename T>
T Load<T>::execute()
{
  #ifdef ATOMIC_DEBUG_MODEL
  std::cout << "Executing Load.." << std::endl;
  #endif
  global_state->record_atomic_operation(this);
  this->build_choose_rf();
  T rf_val = this->get_rf_value(this->rf);
  return rf_val;
}



/**
 * 
 * @brief prepare the atomic store operations
 * 
 */
template<class T>
Store<T>::Store(location store_address, T store_value,
		operation_order store_order,thread_id tid)
  : store_value(store_value),
    Atomic_Operation(store_address, store_order, operation_type::store)
{
  #ifdef ATOMIC_MODEL_DEBUG
  std::cout << "Store::Store : Storing at the atomic location : "
	    << store_address << "with order : " << store_order
	    << "store value : " << store_value
	    << "thread_id : " << tid
	    << std::endl;
  #endif
      
  sequence_number seq_no = global_state->get_sequence_number();
  set_sequence_number(seq_no);
  global_state->record_thread(this, tid);
  create_cv();
}



/**
 * @brief record the atomic store operation
 **/
template<class T>
void Store<T>::execute()
{
  global_state->record_atomic_operation(this);
}



/**
 * @brief records the rmw operation
 * 
 **/
template<class T>
RMW<T>::RMW(location load_store_address, T expected,
	    operation_order success_order,T desired,
	    operation_order failure_order, thread_id tid)
  : expected(expected),
    desired(desired),
    success_mo(success_order),
    failure_mo(failure_order),
    Atomic_Operation(load_store_address, success_order, operation_type::rmw)
{
  sequence_number seq_no = global_state->get_sequence_number();
  set_sequence_number(seq_no);
  global_state->record_thread(this, tid);
  create_cv();
}



/**
 * @brief the operation reads the last value from the modification order
 *
 **/
template<class T>
void RMW<T>::execute()
{
  //get the last store to the location
  Atomic_Operation *last_str = global_state->get_last_store(loc);
  
  T old_value = this->get_rf_value(last_str);
  
  if (old_value == this->expected) {
     this->oper_type = operation_type::rmw;
     this->oper_order = success_mo;
     this->return_value = true;
     this->rmw_value = desired;
   } else {
     this->oper_type = operation_type::rmwr;
     this->oper_order = failure_mo;
     this->expected = old_value;
     return_value = false;
     rmw_value = old_value;
   }

  this->rf = last_str;
  
  if (this->is_acquire())
    {
      ClockVector *str_rf_cv = get_hb_from_store(this->rf);
      if (str_rf_cv != nullptr)
	{
	  this->get_cv()->merge(str_rf_cv);
	}
    }

  this->rf_cv = this->rf->get_cv();
  global_state->record_atomic_operation(this);
}



template<class T>
bool RMW<T>::get_return_value()
{
  return this->return_value;
}


template<class T>
T RMW<T>::get_value()
{
  return this->rmw_value;
}



/**
 *
 * 
 **/
template<typename T>
T RMW<T>::get_rf_value(Atomic_Operation *rf_oper)
{
  T rf_val;
  if (rf_oper->is_store())
    {
      rf_val = ((Store<T>*)rf_oper)->get_value();
    }
  else if (rf_oper->is_rmw())
    {
      rf_val = ((RMW<T>*)rf_oper)->get_value();
    }
  else if (rf_oper->is_fop())
    {
      rf_val = ((FetchOp<T>*)rf_oper)->get_value();
    }
  else if (rf_oper->is_xchange())
    {
      rf_val = ((Xchange<T>*)rf_oper)->get_value();
    }

  return rf_val;
}



/**
 * @brief records the rmw operation
 * 
 **/
template<class T>
Xchange<T>::Xchange(location load_store_address, operation_order xchng_order,
	    T xchng_value, thread_id tid)
  : xchange_value(xchng_value),
    Atomic_Operation(load_store_address, xchng_order, operation_type::xchange)
{
  sequence_number seq_no = global_state->get_sequence_number();
  set_sequence_number(seq_no);
  global_state->record_thread(this, tid);
  create_cv();
}



/**
 * @brief the operation reads the last value from the modification order
 *
 **/
template<class T>
void Xchange<T>::execute()
{
  //get the last store to the location
  Atomic_Operation *last_str = global_state->get_last_store(this->loc);

  T old_value = this->get_rf_value(last_str);
  this->rf = last_str;
  this->return_value = old_value;

    if (this->is_acquire())
    {
      ClockVector *str_rf_cv = get_hb_from_store(this->rf);
      if (str_rf_cv != nullptr)
	{
	  this->get_cv()->merge(str_rf_cv);
	  this->rf_cv = this->get_cv();
	}
    }
  
  global_state->record_atomic_operation(this);
}



/**
 *
 * 
 **/
template<typename T>
T Xchange<T>::get_rf_value(Atomic_Operation *rf_oper)
{
  T rf_val;
  if (rf_oper->is_store())
    {
      rf_val = ((Store<T>*)rf_oper)->get_value();
    }
  else if (rf_oper->is_rmw())
    {
      rf_val = ((RMW<T>*)rf_oper)->get_value();
    }
  else if (rf_oper->is_fop())
    {
      rf_val = ((FetchOp<T>*)rf_oper)->get_value();
    }
  else if (rf_oper->is_xchange())
    {
      rf_val = ((Xchange<T>*)rf_oper)->get_value();
    }

  return rf_val;
}



template<class T>
T Xchange<T>::get_return_value()
{
  return this->return_value;
}


template<class T>
T Xchange<T>::get_value()
{
  return this->xchange_value;
}



template<class T>
FetchOp<T>::FetchOp(location load_store_address, value operand,
		    operation_order success_order, thread_id tid, binary_op bop)
  : bop(bop),
    operand(operand),
    Atomic_Operation(load_store_address, success_order, operation_type::fop)
{
  sequence_number seq_no = global_state->get_sequence_number();
  set_sequence_number(seq_no);
  global_state->record_thread(this, tid);
  create_cv();
}
 


/**
 * @brief executes the atomic fetch and binary operation
 * 
 **/
template<class T>
void FetchOp<T>::execute()
{
  //get the last store to the location
  Atomic_Operation *last_str = global_state->get_last_store(loc);
  
  T old_value, new_value;

  old_value = this->get_rf_value(last_str);
  
  
  switch (bop) {
  case binary_op::add_op:
    new_value = old_value + operand;
    break;
  case binary_op::sub_op:
    new_value = old_value - operand;
    break;
  case binary_op::and_op:
    new_value = old_value & operand;
    break;
  case binary_op::or_op:
    new_value = old_value | operand;
    break;
  case binary_op::xor_op:
    new_value = old_value ^ operand;
    break;
  default:
    std::cout << "FetchOp::execute : Unknown binary operation" << std::endl;
    break;
  }
  this->fop_val = new_value;
  this->return_value = old_value;

  this->rf = last_str;
  
  if (this->is_acquire())
    {
      ClockVector *str_rf_cv = get_hb_from_store(this->rf);
      if (str_rf_cv != nullptr)
	{
	  this->get_cv()->merge(str_rf_cv);
	}
    }

  this->rf_cv = this->rf->get_cv(); 
  global_state->record_atomic_operation(this);
}


template<class T>
T FetchOp<T>::get_return_value()
{
  return this->return_value;
}


template<class T>
T FetchOp<T>::get_value()
{
  return this->fop_val;
}



// TODO: remove the redundant code
template<typename T>
T FetchOp<T>::get_rf_value(Atomic_Operation *rf_oper)
{
  T rf_val;
  if (rf_oper->is_store())
    {
      rf_val = ((Store<T>*)rf_oper)->get_value();
    }
  else if (rf_oper->is_rmw())
    {
      rf_val = ((RMW<T>*)rf_oper)->get_value();
    }
  else if (rf_oper->is_fop())
    {
      rf_val = ((FetchOp<T>*)rf_oper)->get_value();
    }
  else if (rf_oper->is_xchange())
    {
      rf_val = ((Xchange<T>*)rf_oper)->get_value();
    }

  return rf_val;
}




Fence::Fence(operation_order fence_order, thread_id tid)
  : Atomic_Operation(0, fence_order, operation_type::fence)
{}



void Fence::execute()
{
}



/**
 *
 * @brief initialise state before each iteration
 * 
 **/
void initialise_global_state(coyote::Scheduler *scheduler)
{
  #ifdef ATOMIC_DEBUG_MODEL
  std::cout << "initialise_global_state : Global State Initialised with "
	    << "sequence_number : " << START_SEQ_NO << std::endl;
  #endif
  
  global_state = new GlobalState(START_SEQ_NO, scheduler);
}



/**
 *
 * @brief clear state after each test iteration
 *
 **/
void reinitialise_global_state()
{
  #ifdef ATOMIC_DEBUG_MODEL
  std::cout << "reinitialise_global_state : Global State Initialised with " \
    "sequence_number : " << START_SEQ_NO << std::endl;
  #endif
  global_state->clear_state();
  global_state->initialise_sequence_number(START_SEQ_NO);
}



GlobalState::GlobalState(sequence_number start_seq_no, coyote::Scheduler *scheduler)
  : seq_no(start_seq_no),
    scheduler(scheduler)
{}



sequence_number GlobalState::get_sequence_number()
{
  return ++seq_no;
}



std::int16_t GlobalState::get_num_of_threads()
{
  return thread_id_map.size();
}



/**
 * Note the modification order is a acyclic chain
 * @brief check if oper_a is earlier to oper_b in the modification order
 *
 **/
bool GlobalState::check_stores_in_order(Atomic_Operation *oper_a,
					Atomic_Operation *oper_b)
{
  location loc = oper_a->get_operation_location();
  operation_list opers = obj_str_map[loc];
  operation_list::iterator oper_iter = opers.begin();

  int index_a = 0, index_b = 0;

  if (oper_a == oper_b)
  {
      return true;
  }

  while (oper_iter != opers.end())
    {
      if (*oper_iter == oper_a)
	{
	  index_a = oper_iter - opers.begin();
	}
      if (*oper_iter == oper_b)
	{
	  index_b = oper_iter - opers.begin();
	}
      if (index_a > index_b) {
	return false;
      }
      oper_iter++;
    }
  return true;
}


/**
 * @brief records the atomic operations
 * - operation by a thread for a location.
 * - operation by a thread
 **/
void GlobalState::record_operation(Atomic_Operation *oper)
{
  location loc = oper->get_operation_location();
  ThreadState* thrd = oper->get_thread();
  
  //records all operations for a thread per location
  thread_operation_list inner_map = this->obj_thread_oper_map[loc];
  inner_map[thrd].push_back(oper);
  this->obj_thread_oper_map[loc] = inner_map;

  //records all the operations for a thread
  operation_list thrd_opers = this->thread_oper_map[thrd];
  thrd_opers.push_back(oper);
  this->thread_oper_map[thrd] = thrd_opers;
}



/**
 * @brief record the atomic operation updating 
 * the thread state and the
 **/
void GlobalState::record_atomic_operation(Atomic_Operation *oper)
{
  operation_type oper_type = oper->get_operation_type();
  //updates datastructures for all all operations
  record_operation(oper);
  
  switch (oper_type) {
  case operation_type::load :
    record_atomic_load(oper);
    break;
  case operation_type::rmwr :
    record_atomic_load(oper);
    break;
  case operation_type::store :
    record_atomic_store(oper);
    break;
  case operation_type::rmw :
    record_atomic_rmw(oper);
    break;
  case operation_type::fop:
    record_atomic_rmw(oper);
    break;
  case operation_type::fence :
    record_atomic_fence(oper);
    break;
  default:
    std::cout << "GlobalState::record_atomic_operation : Unknown operation type"
	      << std::endl;
    break;
  }
}



/**
 * @brief records a new thread and returns the state
 * or returns exisiting thread state
 *
 **/
void GlobalState::record_thread(Atomic_Operation *oper, thread_id thrd_id)
{
  ThreadState *thrd_state = nullptr;
  location loc = oper->get_operation_location();
  
  if (thread_id_map.find(thrd_id) == thread_id_map.end()) {//new thread
    //map external thread it to internal
    thread_id new_thread_id = thread_id_map.size() + 1;
    thread_id_map[thrd_id] = new_thread_id;
    //map the internal thread id to thread state
    thrd_state = new ThreadState(new_thread_id);
    thread_id_obj_map[new_thread_id] = thrd_state;
  } else {//existing thread state
    thrd_state = thread_id_obj_map[thread_id_map[thrd_id]];
  }

  //assert(thrd_state, ErrorCode::Success);
  
  oper->set_thread(thrd_state);

  print_thread_map();
  
  //last thread operation
  thread_operation inner_map = obj_thread_last_oper_map[loc];
  inner_map[thrd_state] = oper;
}



/**
 *
 * @brief record thread store operations and modification order
 * 
 **/
void GlobalState::record_atomic_store(Atomic_Operation *oper)
{
  location loc = oper->get_operation_location();
  thread_operation_list inner_map = obj_thread_str_map[loc];
  
  inner_map[oper->get_thread()].push_back(oper);
  obj_thread_str_map[loc] = inner_map;

  //record modification order
  obj_str_map[loc].push_back(oper);

  //record the seq_cst store
  if (oper->is_seq_cst()) {
    obj_last_seq_map[loc] = oper;
  }

  #ifdef ATOMIC_MODEL_DEBUG
  std::cout << "GlobalState::record_atomic_store map size : "
	    << inner_map.size()) << std::endl;
  #endif
}



/**
 * 
 * @brief record thread load operations
 *
 **/
void GlobalState::record_atomic_load(Atomic_Operation *oper)
{
  location loc = oper->get_operation_location();
  thread_operation_list inner_map = obj_thread_ld_map[loc];
  
  inner_map[oper->get_thread()].push_back(oper);
  obj_thread_ld_map[loc] = inner_map;

  #ifdef ATOMIC_MODEL_DEBUG
  std::cout << "GlobalState::record_atomic_load/rmwr map size : "
	    << inner_map.size()) << std::endl;
  #endif
}



void GlobalState::record_atomic_rmw(Atomic_Operation *oper)
{
  location loc = oper->get_operation_location();
  record_atomic_store(oper);
    
  thread_operation_list inner_map = obj_thread_rmw_map[loc];
  inner_map[oper->get_thread()].push_back(oper);
}



void GlobalState::record_atomic_fence(Atomic_Operation *oper)
{}



Atomic_Operation* GlobalState::get_last_store(location loc)
{
  if (obj_str_map.find(loc) == obj_str_map.end()) {
    #ifdef ATOMIC_DEBUG_MODEL
    std::cout << "GlobalState::get_last_store : Location Not Found "
	      << std::endl;
    #endif
    return nullptr;
  }

  operation_list opers = obj_str_map[loc];

  if (!opers.empty()) {
    Atomic_Operation *latest_store = opers.back();
    #ifdef ATOMIC_DEBUG_MODEL
    std::cout << "GloabalState::get_last_store : latest_store_seq_no " 
        << latest_store->get_sequence_number() << std::endl;
    #endif
    return latest_store;
  }

  #ifdef ATOMIC_DEBUG_MODEL
  std::cout << "GlobalState::get_last_store : Store Not Found "
	    << std::endl;
  #endif
  return nullptr;
}



/**
 * @brief returns the latest seq_cst store to the location
 *
 **/
Atomic_Operation* GlobalState::get_last_seq_cst_store(location loc)
{
  if (obj_last_seq_map.find(loc) == obj_last_seq_map.end()) {
    #ifdef ATOMIC_DEBUG_MODEL
    std::cout << "GlobalState::get_last_seq_cst_store : Location Not Found "
	      << std::endl;
    #endif
    return nullptr;
  }
  Atomic_Operation *seq_cst_str = obj_last_seq_map[loc];
  return seq_cst_str;
}



/**
 *
 * @brief prints the stores per location per thread 
 **/
void GlobalState::print_thread_stores()
{
  #ifdef ATOMIC_DEBUG_MODEL
  obj_thread_operation_map::iterator outer_it = obj_thread_str_map.begin();
  for (auto i = outer_it; i != obj_thread_str_map.end(); i++)
    {
      std::cout << "GlobalState::print_thread_stores : Location : "
		<< i->first << std::endl;

      thread_operation_list ::iterator inner_it = outer_it->second.begin();

      std::cout << "GlobalState::print_thread_stores inner_map_len : "
		<< outer_it->second.size() << std::endl;

      for(auto j = inner_it; j != outer_it->second.end(); j++ )
	{
	  std::cout << "GlobalState::print_thread_stores : thread : "
		    << j->first->get_id() << std::endl;

	  std::cout << "GlobalState::print_thread_stores : number of stores : "
		    << j->second.size() << std::endl;

	  for(auto k = j->second.begin(); k != j->second.end(); k++)
	    {
	      std::cout << "GlobalState::print_thread_stores : store_seq_no: "
			<< (*k)->get_sequence_number() << std::endl;
	    }
	}
    }
  #endif
}



void GlobalState::print_thread_map()
{
  #ifdef ATOMIC_DEBUG_MODEL
  std::unordered_map<thread_id, ThreadState*>::iterator tmap_it =
    thread_id_obj_map.begin();
  for (auto i = tmap_it; i != thread_id_obj_map.end(); i++)
    {
      std::cout << "GlobalState::print_thread_map : ext_tid : "
		<< i->first << " int_tid : " << i->second->get_id() << std::endl;
    }
  #endif
}

void GlobalState::clear_state()
{
    thread_id_map.clear();
    thread_id_obj_map.clear();
    obj_str_map.clear();
    obj_thread_oper_map.clear();
    obj_thread_str_map.clear();
    obj_thread_ld_map.clear();
    obj_thread_rmw_map.clear();
    obj_thread_last_oper_map.clear();
    obj_last_seq_map.clear();
}
