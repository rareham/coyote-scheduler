#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <unordered_map>
#include <utility>
#include <vector>

#include "state.h"


void initialise_global_state()
{

  assert(!global_state && "initialise_global_state : Global state already initialised");
  #ifdef ATOMIC_DEBUG_MODEL
  printf("initialise_global_state : Global State Initialised with " \
	 "sequence_number : %d \n", START_SEQ_NO);
  #endif
  
  global_state = new GlobalState(START_SEQ_NO);
}



GlobalState::GlobalState(sequence_number start_seq_no) : seq_no(start_seq_no)
{}



sequence_number GlobalState::get_sequence_number()
{
  return seq_no++;
}



std::int16_t GlobalState::get_num_of_threads()
{
  return thread_id_map.size();
}



/**
 *
 *
 * 
 **/
void GlobalState::record_operation(Operation *oper)
{
  location loc = oper->get_operation_location();
  // TODO: Just capturing store operations for a location
  if(oper->get_operation_type() == operation_type::store) {
      loc_operations[loc].push_back(oper);
  }
  
  thread_operation_list inner_map = obj_oper_thrd_map[loc];
  inner_map[oper->get_thread()].push_back(oper);

  if (oper->is_seq_cst()) {
    obj_last_seq_thread_map[loc] = oper;
  }
}



/**
 * @brief record the atomic operation updating 
 * the thread state and the
 **/
void GlobalState::record_atomic_operation(Operation *oper)
{
  assert(oper);
  operation_type oper_type = oper->get_operation_type();
  record_operation(oper);
  
  switch (oper_type) {
  case operation_type::load :
    record_atomic_load(oper);
    break;
  case operation_type::store :
    record_atomic_store(oper);
    break;
  case operation_type::rmw :
    record_atomic_rmw(oper);
    break;
  case operation_type::fence :
    record_atomic_fence(oper);
    break;
  default:
    printf("GlobalState::record_atomic_operation : Unknown operation type\n");
    break;
  }
}



/**
 * @brief records a new thread and returns the state
 * or returns exisiting thread state
 *
 **/
void GlobalState::record_thread(Operation *oper, thread_id thrd_id)
{
  ThreadState *thrd_state = nullptr;
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
  oper->set_thread(thrd_state);

  //last thread operation
  thread_operation inner_map =	\
    obj_last_oper_thread_map[oper->get_operation_location()];
  inner_map[thrd_state] = oper;
}



void GlobalState::record_atomic_store(Operation *oper)
{
  location loc = oper->get_operation_location();
  thread_operation_list inner_map = obj_str_thread_map[loc];
  inner_map[oper->get_thread()].push_back(oper);
} 



void GlobalState::record_atomic_load(Operation *oper)
{}



void GlobalState::record_atomic_rmw(Operation *oper)
{
  location loc = oper->get_operation_location();
  thread_operation_list inner_map = obj_rmw_thread_map[loc];
  inner_map[oper->get_thread()].push_back(oper);
}



void GlobalState::record_atomic_fence(Operation *oper)
{}



Operation* GlobalState::get_last_store(location loc)
{
  if (loc_operations.find(loc) == loc_operations.end()) {
    #ifdef ATOMIC_DEBUG_MODEL
      printf("GlobalState::get_last_store : Location Not Found \n");
    #endif
    return nullptr;
  }
  std::vector<Operation*> opers = loc_operations[loc];
  for (int i = opers.size(); i > 0; i--) {
    if (opers[i]->get_operation_type() == operation_type::store) {
      return opers[i];
    }
  }
  #ifdef ATOMIC_DEBUG_MODEL
    printf("GlobalState::get_last_store : Store Not Found \n");
  #endif
  return nullptr;
}



ThreadState::ThreadState(thread_id thrd_id)
  : thrd_id(thrd_id)
{}



Operation::Operation(location address, memory_order oper_order, operation_type oper_type)
  : loc(address), oper_order((operation_order)oper_order), oper_type(oper_type)
{
  if ((operation_order)oper_order == operation_order::seq_cst) {
    seq_cst = true;
  }
}



Load::Load(location load_address, memory_order load_order, thread_id tid)
  : Operation(load_address, load_order, operation_type::load)
{
  #ifdef ATOMIC_MODEL_DEBUG
    printf("Load::Load : Loading from the atomic location : %x \n with order, \
    value and  tid : %ul %ul\n",load_address, load_order, tid);
  #endif

  assert(load_address && "Load::Load : load_address is NULL");

  sequence_number seq_no = global_state->get_sequence_number();
  set_sequence_number(seq_no);
  global_state->record_thread(this, tid);
  Load::create_cv();
}



void Load::execute()
{
  global_state->record_atomic_operation(this);
  build_rf_set();
}



/**
 *
 *
 * 
 **/
void Load::build_rf_set()
{}



void Store::create_cv()
{
  cv = new ClockVector(this);
}



std::vector<value>* Load::get_rf_set()
{
  return nullptr;
}


/**
 * @brief prepare the atomic store operations
 *
 * 
 */
Store::Store(location store_address, value store_value, memory_order store_order, \
	     thread_id tid) : store_value(store_value),
			      Operation(store_address, store_order,operation_type::store)
{
  #ifdef ATOMIC_MODEL_DEBUG
    printf("Store::Store : Storing at the atomic location : %x \n with order, \
    value and  tid : %ul %ul %ul\n",store_address, store_order, store_value, tid);
  #endif

  assert(store_address && "Store::Store : store_address is NULL");

  sequence_number seq_no = global_state->get_sequence_number();
  set_sequence_number(seq_no);
  global_state->record_thread(this, tid);
  Store::create_cv();
}



void Store::create_cv()
{
  cv = new ClockVector(this);
}

/**
 * record the atomic store operation
 **/
void Store::execute()
{
  global_state->record_atomic_operation(this);
}


/**
 * @brief records the rmw operation
 * 
 **/
RMW::RMW(location load_store_address, value expected, memory_order success_order, \
	 value desired, memory_order failure_order, thread_id tid)
  : expected(expected),
    desired(desired),
    success_mo(success_order),
    failure_mo(failure_order),
    Operation(load_store_address, success_order, operation_type::rmw)
{
  sequence_number seq_no = global_state->get_sequence_number();
  set_sequence_number(seq_no);
  global_state->record_thread(this, tid);
  create_cv();
}



void RMW::create_cv()
{
  cv = new ClockVector(this);
}



/**
 * @brief the operation reads the last value from the modification order
 *
 **/
void RMW::execute()
{
  global_state->record_atomic_operation(this);

  //get the last store to the location
  Store *last_str = (Store*)global_state->get_last_store(loc);

  assert(last_str && "RMW::execute : last_str not found");
  
  value old_value = last_str->get_value();

  if (old_value == expected) {
    Store *str_oper = new Store(loc ,desired, success_mo, get_thread()->get_id());
    str_oper->execute();
    return_value = true;
    return;
  } else {
    expected = old_value;
  }
  return_value = false;
  return ;
}



bool RMW::get_return_value()
{
  return return_value;
}



FetchOp::FetchOp(location load_store_address, value operand, memory_order success_order, \
		thread_id, binary_op bop)
  : bop(bop),
    operand(operand),
    Operation(load_store_address, success_order, operation_type::rmw)
{
  
}



void FetchOp::create_cv()
{
  cv = new ClockVector(this);
}



/**
 * @brief executes the atomic fetch and binary operation
 * 
 **/
void FetchOp::execute()
{
  global_state->record_atomic_operation(this);

  //get the last store to the location
  Store *last_str = (Store*)global_state->get_last_store(loc);
  assert(last_str && "FetchOp::execute : last_str not found");
    
  value old_value = last_str->get_value();
  value new_value;
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
    printf("FetchOp::execute : Unknown binary operation");
    break;
  }
  Store *str_oper = new Store(loc ,new_value, success_mo, get_thread()->get_id());
  str_oper->execute();
  return_value = new_value;
}



Fence::Fence(memory_order fence_order, thread_id tid)
  : Operation(0, fence_order, operation_type::fence)
{}



void Fence::execute()
{
}



ClockVector::ClockVector(Operation *act) : clock_vector(MAX_THREADS, 0)
{
  if (act != NULL)
    clock_vector[act->get_thread()->get_id()] = act->get_sequence_number();
}



ClockVector::~ClockVector()
{}



bool ClockVector::merge(ClockVector *cv)
{
  assert(cv);
  
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
  assert(cv);

  bool changed = false;
  for (int i = 0;i < cv->num_threads;i++)
    if (cv->clock_vector[i] < clock_vector[i]) {
      clock_vector[i] = cv->clock_vector[i];
      changed = true;
    }

  return changed;
}



bool ClockVector::synchronized_since(Operation *act)
{
  int tid = act->get_thread()->get_id();
  if (tid < num_threads)
    return act->get_sequence_number() <= clock_vector[tid];
  return false;
}



logical_clock ClockVector::getClock(thread_id tid)
{
  if (tid < num_threads)
    return clock_vector[tid];
  else
    return 0;
}



void ClockVector::print()
{
  /*int i;
  model_print("(");
  for (i = 0;i < num_threads;i++)
    printf("%2u%s", clock_vector[i], (i == num_threads - 1) ? ")\n" : ", ");
  */
}
