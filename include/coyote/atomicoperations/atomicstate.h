#pragma once

#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "../scheduler.h"
#include "atomicmodel.h"

//@brief set to view debug statements for model functions
//#define ATOMIC_MODEL_DEBUG

//@brief start of the global sequence number
#define START_SEQ_NO 0

class GS;//@brief global state
class TS;//@brief thread state
class AO;//@brief atomic operation
template<typename T>
class STR;//@brief atomic store
template<typename T>
class LD;//@brief atomic load
template<typename T>
class RMW;//@brief atomic rmw
template<typename T>
class XCHNG;//@brief atomic cas
template<typename T>
class FOP;  //@brief atomic cas
class MOG;  //@brief modification order graph
class MON;  //@brief modification order node
class Fence;//@brief fence
class CV;   //@brief clock vector

typedef std::uint64_t value;          //@brief value stored at the address
typedef std::uintptr_t location;      //@brief the address of the variable
typedef std::uint32_t sequence_number;//@brief global sequence number
typedef std::uint64_t thread_id;      //@brief thread id performing atomic operations
typedef std::uint32_t logical_clock;  //@brief logical clock for the vector clocks

typedef std::unordered_map<TS *, std::vector<AO *>>
        thread_operation_list;//@brief operations performed by threads

typedef std::vector<AO *> operation_list;

typedef std::unordered_map<TS *, AO *>
        thread_operation;//@brief an operation performed by threads

typedef std::unordered_map<location, thread_operation_list>
        obj_thread_operation_map;


enum operation_type { load,
                      store,
                      rmw,
                      fop,
                      rmwr,
                      xchange,
                      fence };

std::vector<std::string> operation_type_str =
        {"load", "store", "rmw",
         "fop", "rmwr", "xchng",
         "fence"};

enum binary_op { add_op,
                 sub_op,
                 and_op,
                 or_op,
                 xor_op };

void initialise_global_state(coyote::Scheduler *);
void initialise_global_state(coyote::Scheduler *, std::uint32_t);
void reinitialise_global_state();
template<typename T>
T get_oper_value(AO *);

//@brief stores state for atomic operations in an execution
GS *global_state = nullptr;

//@brief default number of threads
std::uint32_t MAX_THREADS = 20;


/**
 * @brief models the thread
 *
 **/
class TS {
private:
    thread_id thrd_id = 0;
    AO *last_oper = nullptr;//@brief last thread operation

public:
    explicit TS(thread_id);
    thread_id get_id() const;
    AO *get_last_operation() const {
        return last_oper;
    }
    void set_last_operation(AO *pAo);
};


/**
 * @brief Captures HB relation across atomic operations
 *
 **/
class CV {
private:
    std::vector<logical_clock> clock_vector;
    std::uint32_t num_threads = 0;

public:
    CV(thread_id, sequence_number);
    bool merge(CV *);
    bool synchronized_since(thread_id, sequence_number);
    std::string print_cv() {
        std::string result;
        for (logical_clock t : clock_vector) {
            result += std::to_string(t) + " ";
        }
        return result;
    }
    void change(CV *pCv, thread_id i);
};


/**
 * @brief MON, modification order node
 * 
 **/
class MON {
public:
    AO *oper = nullptr;
    std::vector<MON *> out_edges;
    std::vector<MON *> in_edges;
    MON *hasRMW = nullptr;
    CV *mo_cv = nullptr;

    explicit MON(AO *);
    ~MON();
    void add_o_edge(MON *);
    MON *get_o_edge(uint32_t);
    uint32_t get_num_o_edges() const;
    MON *get_i_edge(uint32_t);
    uint32_t get_num_i_edges();
    void setRMW(MON *);
    MON *getRMW() const;
    AO *getOperation() const { return oper; }
    void remove_i_edge(MON *src);
    void print();
};


/**
 * @brief MOG, modification order graph
 *
 **/
class MOG {
public:
    MOG();
    ~MOG();
    void add_o_edges(std::vector<AO *> &, AO *);
    void add_o_edge(AO *, AO *, bool);
    void addRMWEdge(AO *, AO *);
    bool checkReachable(MON *, MON *);
    bool checkReachable(AO *, AO *);
    MON *getNode_noCreate(AO *);

    void print();

private:
    void addNodeEdge(MON *, MON *, bool);
    void putNode(AO *, MON *);
    MON *getNode(AO *);
    std::unordered_map<AO *, MON *> operToNode;
    std::vector<MON *> *queue = nullptr;
};


/**
 * @brief Base class for all the atomic operations
 *
 **/
class AO {
protected:
    TS *thread_state = nullptr;
    location loc;
    operation_type oper_type;
    operation_order oper_order = operation_order::seq_cst;
    //@brief sequence number of the operation
    sequence_number seq_no = 0;
    //@brief true if the operation is seq_cst
    bool seq_cst = false;
    //@brief clock vector associated with the operation
    CV *cv = nullptr;
    //@brief the reads from cv of the store the loads reads from
    CV *rf_cv = nullptr;
    //@brief the reads from store/rmw for a load/rmw
    AO *rf = nullptr;

public:
    AO(location, operation_order, operation_type);

    void set_sequence_number(sequence_number number) {
        this->seq_no = number;
    }

    void set_thread(TS *thrd_state) {
        thread_state = thrd_state;
    }

    CV *get_cv() {
        return cv;
    }

    CV *get_rf_cv() {
        return rf_cv;
    }

    sequence_number get_sequence_number() const {
        return seq_no;
    }

    operation_order get_operation_order() {
        return oper_order;
    }

    operation_type get_operation_type() {
        return oper_type;
    }

    location get_operation_location() const {
        return loc;
    }

    TS *get_thread() {
        return thread_state;
    }

    AO *get_rf() {
        return rf;
    }

    bool is_seq_cst() {
        seq_cst = (oper_order == operation_order::seq_cst);
        return seq_cst;
    }

    void set_rf_cv(CV *pcv) {
        rf_cv = pcv;
    }

    void set_thread_state(thread_id);

    bool happens_before(AO *);

    void create_cv();

    std::uint32_t choose_index(operation_list &);

    void choose_validate(operation_list &);

    bool validate_rf(AO *, operation_list &);

    void update_ld_mo_graph(operation_list &);

    void update_str_mo_graph();

    void build_choose_rf();

    bool is_acquire() {
        if (oper_order == operation_order::acquire || oper_order == operation_order::acq_rel || oper_order == operation_order::seq_cst) {
            return true;
        }
        return false;
    }

    bool is_release() {
        if (oper_order == operation_order::release || oper_order == operation_order::acq_rel || oper_order == operation_order::seq_cst) {
            return true;
        }
        return false;
    }

    bool is_store() {
        if (oper_type == operation_type::rmw || oper_type == operation_type::store) {
            return true;
        }
        return false;
    }

    bool is_load() {
        if (oper_type == operation_type::rmwr || oper_type == operation_type::load) {
            return true;
        }
        return false;
    }

    bool is_rmw() {
        if (oper_type == operation_type::rmw) {
            return true;
        }
        return false;
    }

    bool is_fop() {
        if (oper_type == operation_type::fop) {
            return true;
        }
        return false;
    }

    bool is_xchange() {
        if (oper_type == operation_type::xchange) {
            return true;
        }
        return false;
    }

    CV *get_hb_from_store(AO *);
    void print();
};


/**
 * @brief atomic load
 *
 **/
template<typename T>
class LD : AO {
public:
    LD(location, operation_order, thread_id);
    void print_rf_set(operation_list &);
    void create_rf_cv();
    T execute();
};


/**
 * @brief atomic store
 *
**/
template<typename T>
class STR : AO {
protected:
    T store_value;

public:
    STR(location, T, operation_order, thread_id);
    void execute();
    T get_value() {
        return this->store_value;
    }
};


/**
 * @brief Atomic CAS operation
 *
 *
 **/
template<typename T>
class RMW : AO {
protected:
    T rmw_value;
    bool return_value = false;
    operation_order success_mo;
    operation_order failure_mo;
    T expected;
    T desired;

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
class XCHNG : AO {
protected:
    T xchange_value;
    T return_value;

public:
    XCHNG(location, operation_order, T, thread_id);
    void execute();
    T get_value();
    T get_return_value();
};


/**
 * @brief Atomic binary operations
 *
 *
 **/
template<typename T>
class FOP : AO {
protected:
    binary_op bop;
    T fop_val;
    T return_value;
    value operand;

public:
    FOP(location, value, operation_order, thread_id, binary_op);
    void execute();
    T get_return_value();
    T get_value();
};


/**
 * @brief Atomic Fence operations
 *
 *
 **/
class Fence : AO {
public:
    Fence(operation_order, thread_id);
    void execute();
};


/**
 * @brief Execution trace of atomic operations
 *
 *
 **/
class GS {
private:
    sequence_number seq_no;
    coyote::Scheduler *scheduler;

    //@brief mapping the thread id with internal id
    std::unordered_map<thread_id, thread_id> thread_id_map;
    //@brief mapping the internal thread id with TS
    std::unordered_map<thread_id, TS *> thread_id_obj_map;
    //@brief mapping a thread to the parent thread
    std::unordered_map<thread_id, TS *> thread_parent_obj_map;
    //@brief all the operations performed at a location
    std::unordered_map<location, operation_list> obj_str_map;
    //@brief thread and operations map - debugging purpose
    std::unordered_map<TS *, operation_list> d_thread_oper_map;
    //@brief execution trace
    operation_list d_exec_trace;
    //@brief all operations
    obj_thread_operation_map obj_thread_oper_map;
    //@brief list of store actions by the thread at a location
    obj_thread_operation_map obj_thread_str_map;
    //@brief list of load actions by the thread at a location
    obj_thread_operation_map obj_thread_ld_map;
    //@brief list of load actions by the thread at a location
    obj_thread_operation_map obj_thread_rmw_map;
    //@brief the latest sequential store action
    std::unordered_map<location, AO *> obj_last_sc_map;

    MOG mo_graph;
    void record_operation(AO *);
    void record_atomic_store(AO *);
    void record_atomic_load(AO *);
    void record_atomic_rmw(AO *);
    void record_atomic_fence(AO *);

public:
    GS(sequence_number, coyote::Scheduler *);

    void initialise_sequence_number(sequence_number global_seq_number) {
        seq_no = global_seq_number;
    }

    sequence_number get_sequence_number();

    /**
     * maps the external thread id to the internal thread id
     * @param tid
     * @return
     */
    thread_id get_thread_id(thread_id tid) {
        try {
            thread_id itid = thread_id_map.at(tid);
            return itid;
        } catch (std::string error) {
            std::cout << "GS::get_thread_id : thread not registered "
                      << tid << std::endl;
            exit(1);
        }
    }

    /**
     * get the thread object for the thread id
     * @param tid
     * @return
     */
    TS *get_thread_obj(thread_id tid) {
        return thread_id_obj_map.at(tid);
    }

    coyote::Scheduler *get_scheduler() {
        return scheduler;
    }

    /**
     * gets the next integer based on the scheduler strategy
     * @param max
     * @return
     */
    int get_next_integer(int max) {
        return scheduler->next_integer(max);
    }

    /**
     * gets the present thread id
     * @return
     */
    int get_scheduled_op_id() {
        return scheduler->scheduled_operation_id();
    }

    std::uint32_t get_num_of_threads();
    AO *get_last_seq_cst_store(location);
    void record_atomic_operation(AO *);
    void record_thread(thread_id parent, thread_id child);
    void add_to_mo_graph(operation_list &, AO *);
    void add_rmw_to_mo_graph(AO *, AO *);

    thread_operation_list get_thread_stores(location loc) {
        return obj_thread_str_map[loc];
    }

    thread_operation_list get_thread_opers(location loc) {
        return obj_thread_oper_map[loc];
    }

    int get_num_threads() {
        return thread_id_map.size();
    }

    bool check_stores_in_order(AO *, AO *);
    void print_modification_order();
    void print_thread_stores();
    void print_thread_map();
    void record_seq_cst_store(AO *);

    void clear_state();
    void print_execution_trace();
    TS *get_parent_thread(thread_id i);
    void set_parent_thread(thread_id child, thread_id parent);
};

TS::TS(thread_id thrd_id)
    : thrd_id(thrd_id) {}

thread_id TS::get_id() const {
    return thrd_id;
}

/**
 * registers the last operation for the thread
 * @param oper
 */
void TS::set_last_operation(AO *oper) {
    this->last_oper = oper;
}


CV::CV(thread_id tid, sequence_number seq_no)
    : clock_vector(MAX_THREADS, 0) {
    num_threads = global_state->get_num_of_threads();
    clock_vector[tid] = seq_no;
}


/**
 * @brief merge this with the cv
 *
 **/
bool CV::merge(CV *cv) {
    bool changed = false;

    for (std::uint32_t i = 0; i < cv->num_threads; i++) {
        if (cv->clock_vector[i] > clock_vector[i]) {
            clock_vector[i] = cv->clock_vector[i];
            changed = true;
        }
    }
    return changed;
}


/**
 * @brief checks if this cv is synchronised with seq_no of the operation
 *
 * Note : each operation is assigned with the sequence no in the respective
 * logical clock of the thread performing the operation
 **/
bool CV::synchronized_since(thread_id tid, sequence_number seq_no) {
    if (tid <= num_threads) {
        return seq_no <= clock_vector[tid];
    }
    return false;
}

/**
 * copy the cv vector from parent till the tid
 * @param pCv
 * @param i
 */
void CV::change(CV *parent_cv, thread_id itid) {
    for (int j = 0; j < itid; ++j) {
        this->clock_vector[j] = parent_cv->clock_vector[j];
    }
}


/**
 * @brief Constructor for a MON - modification order node
 * @param oper The AO for this node
 */
MON::MON(AO *oper)
    : oper(oper),
      hasRMW(nullptr),
      mo_cv(new CV(oper->get_thread()->get_id(), oper->get_sequence_number())) {}


MON::~MON() {
    delete mo_cv;
}


void MON::remove_i_edge(MON *src) {
    for (uint32_t i = 0; i < in_edges.size(); i++) {
        if (in_edges[i] == src) {
            in_edges[i] = in_edges[in_edges.size() - 1];
            in_edges.pop_back();
            break;
        }
    }
}


MON *MON::get_o_edge(uint32_t i) {
    return out_edges[i];
}


uint32_t MON::get_num_o_edges() const {
    return out_edges.size();
}


MON *MON::get_i_edge(uint32_t i) {
    return in_edges[i];
}


uint32_t MON::get_num_i_edges() {
    return in_edges.size();
}


void MON::add_o_edge(MON *node) {
    for (auto &out_edge : out_edges) {
        if (out_edge == node) {
            return;
        }
    }

    out_edges.push_back(node);
    node->in_edges.push_back(this);
}


MON *MON::getRMW() const {
    return hasRMW;
}


void MON::setRMW(MON *node) {
    if (hasRMW != nullptr) {
        return;
    }
    hasRMW = node;
}

void MON::print() {
    this->oper->print();
}

MOG::MOG() : queue(new std::vector<MON *>()) {}

MOG::~MOG() {
    delete queue;
}

void MOG::putNode(AO *oper, MON *node) {
    if (operToNode.find(oper) == operToNode.end()) {
        operToNode[oper] = node;
    }
}


MON *MOG::getNode_noCreate(AO *oper) {
    if (operToNode.find(oper) == operToNode.end()) {
        return nullptr;
    }
    return operToNode[oper];
}


MON *MOG::getNode(AO *oper) {
    MON *node = getNode_noCreate(oper);
    if (node == nullptr) {
        node = new MON(oper);
        putNode(oper, node);
    }
    return node;
}


/**
 * @brief adds edges in the mograph
 *  
 * refer to rmw and coherence constraints from CDSChecker
 **/
void MOG::addNodeEdge(MON *fromnode, MON *tonode, bool forceedge) {
    if (checkReachable(fromnode, tonode) && !forceedge) {
        return;
    }

    while (MON *nextnode = fromnode->getRMW()) {
        if (nextnode == tonode) {
            break;
        }
        fromnode = nextnode;
    }

    fromnode->add_o_edge(tonode);

    //propagating the cv changes in the dag
    if (tonode->mo_cv->merge(fromnode->mo_cv)) {
        queue->push_back(tonode);
        while (!queue->empty()) {
            MON *node = queue->back();
            queue->pop_back();
            uint32_t numedges = node->get_num_o_edges();
            for (uint32_t i = 0; i < numedges; i++) {
                MON *enode = node->get_o_edge(i);
                if (enode->mo_cv->merge(node->mo_cv)) {
                    queue->push_back(enode);
                }
            }
        }
    }
}


void MOG::addRMWEdge(AO *from, AO *rmw) {
    MON *fromnode = getNode(from);
    MON *rmwnode = getNode(rmw);

    fromnode->setRMW(rmwnode);

    for (uint32_t i = 0; i < fromnode->get_num_o_edges(); i++) {
        MON *tonode = fromnode->get_o_edge(i);
        if (tonode != rmwnode) {
            rmwnode->add_o_edge(tonode);
        }
        tonode->remove_i_edge(fromnode);
    }
    fromnode->out_edges.clear();

    addNodeEdge(fromnode, rmwnode, true);
}


void MOG::add_o_edges(operation_list &edgeset, AO *to) {
    if (edgeset.empty()) {
        return;
    }

    operation_list to_be_erased;

    for (int i = 0; i < edgeset.size(); ++i) {
        MON *node = getNode(edgeset[i]);
        for (int j = i + 1; j < edgeset.size(); ++j) {
            MON *node2 = getNode(edgeset[j]);
            if (checkReachable(node, node2)) {
                to_be_erased.push_back(edgeset[i]);
                break;
            } else if (checkReachable(node2, node)) {
                to_be_erased.push_back(edgeset[j]);
                continue;
            }
        }
    }

    for (auto &i : edgeset) {
        if (std::find(to_be_erased.begin(), to_be_erased.end(), i) == to_be_erased.end()) {
            add_o_edge(i, to, i->get_thread()->get_id() == to->get_thread()->get_id());
        }
    }

    edgeset.clear();
}


void MOG::add_o_edge(AO *from, AO *to, bool forceedge) {
    MON *fromnode = getNode(from);
    MON *tonode = getNode(to);

    addNodeEdge(fromnode, tonode, forceedge);
}


bool MOG::checkReachable(MON *from, MON *to) {
    return to->mo_cv->synchronized_since(from->oper->get_thread()->get_id(),
                                         from->oper->get_sequence_number());
}


bool MOG::checkReachable(AO *from, AO *to) {
    MON *fromnode = getNode_noCreate(from);
    MON *tonode = getNode_noCreate(to);

    if (!fromnode || !tonode) {
        return false;
    }

    return checkReachable(fromnode, tonode);
}

/**
 * dump the modification order graph
 */
void MOG::print() {
#ifdef ATOMIC_MODEL_DEBUG
    int len = this->queue->size();
    for (int i = 0; i < len; ++i) {
        MON *i_node = this->queue->at(i);
        std::cout << "Node : " << std::endl;
        i_node->print();
        std::cout << "OutEdges : " << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        for (int j = 0; j < i_node->out_edges.size(); ++j) {
            MON *j_node = i_node->out_edges.at(j);
            j_node->print();
        }
    }
#endif
}


AO::AO(location address, operation_order oper_order, operation_type oper_type)
    : loc(address),
      oper_type(oper_type),
      oper_order(oper_order) {
    if ((operation_order) oper_order == operation_order::seq_cst) {
        seq_cst = true;
    }
}

/**
 * sets the thread state for the operation
 * @param tid
 */
void AO::set_thread_state(thread_id tid) {
    thread_id itid = global_state->get_thread_id(tid);
    TS *thrd_state = global_state->get_thread_obj(itid);
    this->set_thread(thrd_state);
}

bool AO::happens_before(AO *oper) {
    return oper->cv->synchronized_since(this->get_thread()->get_id(),
                                        this->get_sequence_number());
}

/**
 * @brief initialises the cv for the operation based on the parent thread
 * or operation
 */
void AO::create_cv() {
    TS *thrd_st = this->get_thread();
    thread_id tid = thrd_st->get_id();
    sequence_number seq_no = this->get_sequence_number();
    //present operation cv
    cv = new CV(tid, seq_no);
    //parent operation
    AO *paren_oper = thrd_st->get_last_operation();
    CV *paren_cv = nullptr;
    if (paren_oper == nullptr) {//if the parent operation is null
        TS *paren_thrd_st = global_state->get_parent_thread(tid);
        AO *thrd_last_oper = paren_thrd_st->get_last_operation();
        if (thrd_last_oper == nullptr) {
            return;
        }
        paren_cv = thrd_last_oper->get_cv();
    } else {
        paren_cv = paren_oper->get_cv();
    }
    //copying parent operation cv to child
    cv->change(paren_cv, tid);
}


/**
 * @brief builds the p_rf clock for the load operation
 *
 * The RF clock is evaluated inductively.
 **/
CV *AO::get_hb_from_store(AO *p_rf) {
    operation_list rmw_rf_set;

    if (p_rf->is_rmw()) {
        AO *reads_from = p_rf->get_rf();
        rmw_rf_set.push_back(p_rf);
    }

    CV *vec = nullptr;
    do {
        if (p_rf->get_rf_cv() != nullptr) {
            vec = p_rf->get_rf_cv();
        } else if (p_rf->is_acquire() && p_rf->is_release()) {
            vec = p_rf->get_cv();
        } else if (p_rf->is_release() && !p_rf->is_rmw()) {
            vec = p_rf->get_cv();
        } else if (p_rf->is_release()) {
            vec = new CV(0, 0);
            vec->merge(p_rf->get_cv());
            p_rf->set_rf_cv(vec);
        } else {
            if (vec == nullptr) {
                if (p_rf->is_rmw()) {
                    vec = new CV(0, 0);
                }
            } else {
                vec = new CV(0, 0);
            }
            p_rf->set_rf_cv(vec);
        }
        if (rmw_rf_set.empty()) {
            break;
        }
        p_rf = rmw_rf_set.front();
        rmw_rf_set.pop_back();
    } while (true);

    return vec;
}


/**
 * @brief chooses an integer within a range, strategically.
 *
 **/
std::uint32_t AO::choose_index(operation_list &rf_set) {
    int index = global_state->get_next_integer(rf_set.size());

#ifdef ATOMIC_MODEL_DEBUG
    std::cout << "AO::choose_index : chosen index " << index
              << std::endl;
#endif
    return index;
}


/**
 *
 * @brief collects all the stores happening before and in happens-before
 *
 * happening before - seen earlier in the trace
 * happens before - have happens-before relation between the operation
 * 
 **/
void AO::build_choose_rf() {
    operation_list rf_set;
    AO *last_sc_store = nullptr;
    location t_loc = this->get_operation_location();

    try {
        thread_operation_list inner_map = global_state->get_thread_stores(t_loc);

        if (this->is_seq_cst()) {
            last_sc_store = global_state->get_last_seq_cst_store(t_loc);
        }

        auto thread_iter = inner_map.begin();

        //each thread
        while (thread_iter != inner_map.end()) {
            operation_list store_opers = thread_iter->second;
            auto store_iter = store_opers.rbegin();
            //each operation
            while (store_iter != store_opers.rend()) {
                AO *store_oper = *store_iter;
                if (store_oper == this) {
                    store_iter++;
                    continue;
                }

                bool allow_rf = true;

                if (this->is_seq_cst() &&
                    (store_oper->is_seq_cst() ||
                     (last_sc_store != nullptr &&
                      store_oper->happens_before(last_sc_store))) &&
                    store_oper != last_sc_store) {
                    allow_rf = false;
                }
                if (allow_rf) {
                    rf_set.push_back(store_oper);
                }
                if (store_oper->happens_before(this)) {
                    break;
                }
                store_iter++;
            }
            thread_iter++;
        }

        this->choose_validate(rf_set);
    } catch (std::string &error) {
        std::cout << error << std::endl;
        return;
    }
}


/**
 * @breif affect the modification order graph for loads
 * 
 **/
void AO::update_ld_mo_graph(operation_list &edgeset) {
    global_state->add_to_mo_graph(edgeset, this);
}


/**
 * @brief affect the modification order graph for stores
 * 
 **/
void AO::update_str_mo_graph() {
    std::vector<AO *> edgeset;
    location t_loc = this->get_operation_location();
    thread_operation_list inner_map = global_state->get_thread_opers(t_loc);
    auto thread_iter = inner_map.begin();

    if (this->is_seq_cst()) {
        AO *last_seq_cst =
                global_state->get_last_seq_cst_store(t_loc);

        if (last_seq_cst != nullptr) {
            edgeset.push_back(last_seq_cst);
        }
        global_state->record_seq_cst_store(this);
    }

    while (thread_iter != inner_map.end()) {
        operation_list opers = thread_iter->second;

        auto oper_iter = opers.rbegin();
        while (oper_iter != opers.rend()) {
            AO *oper = *oper_iter;

            if (oper == this) {
                if (this->is_rmw()) {
                    if (this->get_rf() != nullptr) {
                        break;
                    } else {
                        oper_iter++;
                        continue;
                    }
                } else {
                    oper_iter++;
                    continue;
                }
            }

            if (oper->happens_before(this)) {
                if (oper->is_store()) {
                    edgeset.push_back(oper);
                } else if (oper->is_load()) {
                    edgeset.push_back(oper->get_rf());
                }
                break;
            }
            oper_iter++;
        }
        thread_iter++;
    }
    global_state->add_to_mo_graph(edgeset, this);
}


/**
 * @brief choose the store from the rf set and validate 
 * if modification order graph introduces no cycles.
 **/
void AO::choose_validate(operation_list &rf_set) {
#ifdef ATOMIC_MODEL_DEBUG
    std::cout << "AO::choose_validate "
              << "choosing the index for reads from"
              << std::endl;
#endif
    if (rf_set.empty()) {
        return;
    }

    //choose a store to read from
    while (true) {
        uint32_t index = choose_index(rf_set);
        AO *rf_store = rf_set[index];
        operation_list edgeset;
        //validate the store for the load
        if (this->validate_rf(rf_store, edgeset)) {
            this->update_ld_mo_graph(edgeset);
            this->rf = rf_store;
            if (this->is_acquire() && !this->is_rmw()) {
                CV *str_rf_cv = get_hb_from_store(rf_store);
                if (str_rf_cv == nullptr) {
                    return;
                }
                this->get_cv()->merge(str_rf_cv);
            }
            this->rf_cv = rf_store->get_cv();

#ifdef ATOMIC_MODEL_DEBUG
            std::cout << "AO::choose_validate : operation synchronised "
                      << this->get_cv()->print_cv()
                      << std::endl;
#endif
            return;
        }

        rf_set[index] = rf_set.back();
        rf_set.pop_back();
        if (rf_set.empty()) {
#ifdef ATOMIC_MODEL_DEBUG
            std::cout << "LD::build_choose_rf : no valid stores found"
                      << std::endl;
#endif
            this->rf = nullptr;
            return;
        }
    }
}


/**
 *
 * @brief Checks if reading from a store is valid
 * 
 * Note that according to C++20 spec, the modification order is defined due to
 * coherence constraints, refer to  section 6.9.2.1(DataRaces) from 14 - 20.
 **/
bool AO::validate_rf(AO *rf, operation_list &edgeset) {
    thread_operation_list inner_map = global_state->get_thread_opers(loc);
    auto thread_iter = inner_map.begin();

#ifdef ATOMIC_MODEL_DEBUG
    std::cout << "AO::validate_rf : validating chosen reads from" << std::endl;
#endif

    while (thread_iter != inner_map.end()) {
        operation_list opers = thread_iter->second;
        //for each operation by the thread, from latest to earliest
        auto oper_iter = opers.rbegin();

        while (oper_iter != opers.rend()) {
            AO *oper = *oper_iter;

            if (oper == this) {
                oper_iter++;
                continue;
            }

            if (oper == rf) {
                if (oper->happens_before(this)) {
                    break;
                } else {
                    oper_iter++;
                    continue;
                }
            }

            if (oper->happens_before(rf)) {
                if (oper->is_store()) {
                    if (!global_state->check_stores_in_order(rf, oper)) {
                        return false;
                    }
                    edgeset.push_back(oper);
                } else {
                    AO *prevrf = oper->get_rf();
                    if (!(prevrf == rf)) {
                        if (global_state->check_stores_in_order(rf, prevrf)) {
                            return false;
                        }
                        edgeset.push_back(prevrf);
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

void AO::print() {
    std::cout << "AO::print oper : " << this->get_sequence_number()
              << " thread_id : " << this->get_thread()->get_id()
              << " cv : " << this->get_cv()->print_cv()
              << " operation_order : " << operation_order_str[this->oper_order]
              << " operation_type : " << operation_type_str[this->oper_type]
              << std::endl;
}

template<typename T>
LD<T>::LD(location load_address, operation_order load_order, thread_id tid)
    : AO(load_address, load_order, operation_type::load) {
    sequence_number seq_no = global_state->get_sequence_number();
    this->set_sequence_number(seq_no);
    this->set_thread_state(tid);
    this->create_cv();
    this->get_thread()->set_last_operation(this);
}


template<typename T>
void LD<T>::create_rf_cv() {
    this->rf_cv =
            new CV(this->get_thread()->get_id(), this->get_sequence_number());
}


/**
 * @brief records the load operation and returns a store based on the 
 * strategy of the scheduler
 *
 **/
template<typename T>
T LD<T>::execute() {
#ifdef ATOMIC_MODEL_DEBUG
    std::cout << "Executing LD.." << std::endl;
#endif
    global_state->record_atomic_operation(this);
    this->build_choose_rf();
#ifdef ATOMIC_MODEL_DEBUG
    global_state->print_modification_order();
#endif
    T rf_val = get_oper_value<T>(this->rf);
    return rf_val;
}


/**
 * @brief dump the reads from set for the load
 * 
 **/
template<typename T>
void LD<T>::print_rf_set(operation_list &rf_set) {
#ifdef ATOMIC_MODEL_DEBUG
    operation_list::iterator rf_set_iter = rf_set.begin();
    while (rf_set_iter != rf_set.end()) {
        AO *rf_oper = *rf_set_iter;

        T rf_val = get_oper_value<T>(rf_oper);

        std::cout << "LD::print_rf_set : location " << rf_oper->get_operation_location()
                  << " value = " << rf_val
                  << std::endl;
        rf_set_iter++;
    }
#endif
}


/**
 * @brief prepare the atomic store operations
 * 
 */
template<typename T>
STR<T>::STR(location store_address, T store_value,
            operation_order store_order, thread_id tid)
    : store_value(store_value),
      AO(store_address, store_order, operation_type::store) {
#ifdef ATOMIC_MODEL_DEBUG
    std::cout << "STR::STR : Storing at the atomic location : " << store_address
              << " with order : " << store_order
              << " store value : " << store_value
              << " thread_id : " << tid
              << std::endl;
#endif

    sequence_number seq_no = global_state->get_sequence_number();
    this->set_sequence_number(seq_no);
    this->set_thread_state(tid);
    this->create_cv();
    this->get_thread()->set_last_operation(this);
}


/**
 * @brief record the atomic store operation
 **/
template<typename T>
void STR<T>::execute() {
    global_state->record_atomic_operation(this);
    this->update_str_mo_graph();
#ifdef ATOMIC_MODEL_DEBUG
    global_state->print_modification_order();
#endif
}


/**
 * @brief records the rmw operation
 * 
 **/
template<typename T>
RMW<T>::RMW(location load_store_address, T expected,
            operation_order success_order, T desired,
            operation_order failure_order, thread_id tid)
    : expected(expected),
      desired(desired),
      success_mo(success_order),
      failure_mo(failure_order),
      AO(load_store_address, success_order, operation_type::rmw) {
    sequence_number seq_no = global_state->get_sequence_number();
    this->set_sequence_number(seq_no);
    this->set_thread_state(tid);
    this->create_cv();
    this->get_thread()->set_last_operation(this);
}


/**
 * @brief the operation reads the last value from the modification order
 *
 **/
template<typename T>
void RMW<T>::execute() {
    this->build_choose_rf();
    T old_value = get_oper_value<T>(this->get_rf());

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

    if (this->is_acquire()) {
        CV *str_rf_cv = get_hb_from_store(this->rf);
        if (str_rf_cv != nullptr) {
            this->get_cv()->merge(str_rf_cv);
        }
    }

    this->rf_cv = this->rf->get_cv();
    global_state->record_atomic_operation(this);

    if (this->is_rmw()) {
        global_state->add_rmw_to_mo_graph(this->rf, this);
        this->update_str_mo_graph();
    }
}


template<typename T>
bool RMW<T>::get_return_value() {
    return this->return_value;
}


template<typename T>
T RMW<T>::get_value() {
    return this->rmw_value;
}


/**
 * @brief records the rmw operation
 * 
 **/
template<typename T>
XCHNG<T>::XCHNG(location load_store_address, operation_order xchng_order,
                T xchng_value, thread_id tid)
    : xchange_value(xchng_value),
      AO(load_store_address, xchng_order, operation_type::xchange) {
    sequence_number seq_no = global_state->get_sequence_number();
    this->set_sequence_number(seq_no);
    this->set_thread_state(tid);
    this->create_cv();
    this->get_thread()->set_last_operation(this);
}


/**
 * @brief the operation reads the last value from the modification order
 *
 **/
template<typename T>
void XCHNG<T>::execute() {
    this->build_choose_rf();
    T old_value = get_oper_value<T>(this->get_rf());

    this->return_value = old_value;

    if (this->is_acquire()) {
        CV *str_rf_cv = get_hb_from_store(this->rf);
        if (str_rf_cv != nullptr) {
            this->get_cv()->merge(str_rf_cv);
            this->rf_cv = this->get_cv();
        }
    }

    global_state->record_atomic_operation(this);

    if (this->is_rmw()) {
        global_state->add_rmw_to_mo_graph(this->rf, this);
        this->update_str_mo_graph();
    }
#ifdef ATOMIC_MODEL_DEBUG
    global_state->print_modification_order();
#endif
}


template<typename T>
T XCHNG<T>::get_return_value() {
    return this->return_value;
}


template<typename T>
T XCHNG<T>::get_value() {
    return this->xchange_value;
}


template<typename T>
FOP<T>::FOP(location load_store_address, value operand,
            operation_order success_order, thread_id tid, binary_op bop)
    : bop(bop),
      operand(operand),
      AO(load_store_address, success_order, operation_type::fop) {
    sequence_number seq_no = global_state->get_sequence_number();
    this->set_sequence_number(seq_no);
    this->set_thread_state(tid);
    this->create_cv();
    this->get_thread()->set_last_operation(this);
}


/**
 * @brief executes the atomic fetch and binary operation
 * 
 **/
template<typename T>
void FOP<T>::execute() {
    this->build_choose_rf();

    T old_value, new_value;
    old_value = get_oper_value<T>(this->get_rf());

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
            std::cout << "FOP::execute : Unknown binary operation" << std::endl;
            break;
    }
    this->fop_val = new_value;
    this->return_value = old_value;

    if (this->is_acquire()) {
        CV *str_rf_cv = get_hb_from_store(this->rf);
        if (str_rf_cv != nullptr) {
            this->get_cv()->merge(str_rf_cv);
        }
    }

    if (this->is_rmw()) {
        global_state->add_rmw_to_mo_graph(this->rf, this);
        this->update_str_mo_graph();
    }

    this->rf_cv = this->rf->get_cv();
    global_state->record_atomic_operation(this);
}


template<typename T>
T FOP<T>::get_return_value() {
    return this->return_value;
}


template<typename T>
T FOP<T>::get_value() {
    return this->fop_val;
}


Fence::Fence(operation_order fence_order, thread_id tid)
    : AO(0, fence_order, operation_type::fence) {
    this->get_thread()->set_last_operation(this);
}


void Fence::execute() {}


/**
 *
 * @brief initialise state before each iteration
 * 
 **/
void initialise_global_state(coyote::Scheduler *scheduler) {
#ifdef ATOMIC_MODEL_DEBUG
    std::cout << "initialise_global_state : Global State Initialised with "
              << "sequence_number : " << START_SEQ_NO << std::endl;
#endif

    global_state = new GS(START_SEQ_NO, scheduler);
}


/**
 *
 * @brief initialise state before each iteration for #threads
 * 
 **/
void initialise_global_state(coyote::Scheduler *scheduler,
                             std::uint32_t no_of_threads) {
#ifdef ATOMIC_MODEL_DEBUG
    std::cout << "initialise_global_state : Global State Initialised with "
              << "sequence_number : " << START_SEQ_NO
              << std::endl;
#endif

    MAX_THREADS = no_of_threads;

    global_state = new GS(START_SEQ_NO, scheduler);
}


/**
 *
 * @brief clear state after each test iteration
 *
 **/
void reinitialise_global_state() {
#ifdef ATOMIC_MODEL_DEBUG
    std::cout << "reinitialise_global_state : Global State Initialised with "
                 "sequence_number : "
              << START_SEQ_NO << std::endl;
#endif
    global_state->clear_state();
    global_state->initialise_sequence_number(START_SEQ_NO);
}


/**
 * @brief returns the computed value from all the operation types
 **/
template<typename T>
T get_oper_value(AO *rf_oper) {
    T rf_val;
    if (rf_oper->is_store()) {
        rf_val = ((STR<T> *) rf_oper)->get_value();
    } else if (rf_oper->is_rmw()) {
        rf_val = ((RMW<T> *) rf_oper)->get_value();
    } else if (rf_oper->is_fop()) {
        rf_val = ((FOP<T> *) rf_oper)->get_value();
    } else if (rf_oper->is_xchange()) {
        rf_val = ((XCHNG<T> *) rf_oper)->get_value();
    }

    return rf_val;
}


GS::GS(sequence_number start_seq_no,
       coyote::Scheduler *scheduler)
    : seq_no(start_seq_no),
      scheduler(scheduler) {}


/**
 * @brief assigns a new sequence number to each operation
 * 
 **/
sequence_number GS::get_sequence_number() {
    return ++seq_no;
}

std::uint32_t GS::get_num_of_threads() {
    return thread_id_map.size();
}

/**
 * Return the parent thread for the present thread
 * @param i
 * @return
 */
TS *GS::get_parent_thread(thread_id tid) {
    return thread_parent_obj_map[tid];
}

void GS::set_parent_thread(thread_id child, thread_id parent) {
    thread_id ictid = this->get_thread_id(child);
    thread_id iptid = this->get_thread_id(parent);
    this->thread_parent_obj_map[ictid] = this->get_thread_obj(iptid);
}

void GS::add_to_mo_graph(operation_list &edgeset, AO *oper) {
    mo_graph.add_o_edges(edgeset, oper);
}


void GS::add_rmw_to_mo_graph(AO *from, AO *to) {
    mo_graph.addRMWEdge(from, to);
}


/**
 * Note the modification order is a acyclic chain
 * @brief check if oper_a is earlier to oper_b in the modification order
 *
 **/
bool GS::check_stores_in_order(AO *oper_a, AO *oper_b) {
#ifdef ATOMIC_MODEL_DEBUG
    std::cout << "GS::check_stores_in_order : checking if the stores are in order"
              << std::endl;
#endif

    if (mo_graph.checkReachable(oper_a, oper_b)) {
        return false;
    }

#ifdef ATOMIC_MODEL_DEBUG
    std::cout << "GS::check_stores_in_order : reachable operations"
              << std::endl;
#endif

    return true;
}


/**
 * @brief records the atomic operations
 * - operation by a thread for a location.
 * - operation by a thread
 **/
void GS::record_operation(AO *oper) {
    location loc = oper->get_operation_location();
    TS *thrd = oper->get_thread();

    //records all operations for a thread per location
    thread_operation_list inner_map = this->obj_thread_oper_map[loc];
    inner_map[thrd].push_back(oper);
    this->obj_thread_oper_map[loc] = inner_map;

    //records all the operations for a thread
    operation_list thrd_opers = this->d_thread_oper_map[thrd];
    thrd_opers.push_back(oper);
    this->d_thread_oper_map[thrd] = thrd_opers;

    //records the execution trace
    this->d_exec_trace.push_back(oper);
}


/**
 * @brief record the atomic operation updating 
 * the thread state and the
 **/
void GS::record_atomic_operation(AO *oper) {
    operation_type oper_type = oper->get_operation_type();
    //updates datastructures for all all operations
    record_operation(oper);

    switch (oper_type) {
        case operation_type::load:
            record_atomic_load(oper);
            break;
        case operation_type::rmwr:
            record_atomic_load(oper);
            break;
        case operation_type::store:
            record_atomic_store(oper);
            break;
        case operation_type::rmw:
            record_atomic_rmw(oper);
            break;
        case operation_type::fop:
            record_atomic_rmw(oper);
            break;
        case operation_type::xchange:
            record_atomic_rmw(oper);
            break;
        case operation_type::fence:
            record_atomic_fence(oper);
            break;
        default:
            std::cout << "GS::record_atomic_operation : Unknown operation type"
                      << std::endl;
            break;
    }
}


/**
 * @brief records a new thread and returns the state
 * or returns existing thread state
 *
 **/
void GS::record_thread(thread_id child, thread_id parent) {
    TS *thrd_state = nullptr;

    if (thread_id_map.find(child) == thread_id_map.end()) {//new thread
        //map external thread it to internal
        thread_id new_thread_id = thread_id_map.size() + 1;
        thread_id_map[child] = new_thread_id;
        //map the internal thread id to thread state
        thrd_state = new TS(new_thread_id);
        thread_id_obj_map[new_thread_id] = thrd_state;
        this->set_parent_thread(child, parent);
    }
    print_thread_map();
}


/**
 *@brief record thread store operations
 * 
 **/
void GS::record_atomic_store(AO *oper) {
    location loc = oper->get_operation_location();
    thread_operation_list inner_map = obj_thread_str_map[loc];

    inner_map[oper->get_thread()].push_back(oper);
    obj_thread_str_map[loc] = inner_map;


#ifdef ATOMIC_MODEL_DEBUG
    std::cout << "GS::record_atomic_store map size : "
              << inner_map.size() << std::endl;
#endif
}


void GS::record_seq_cst_store(AO *oper) {
    location loc = oper->get_operation_location();
    if (oper->is_seq_cst()) {
        obj_last_sc_map[loc] = oper;
    }
}


/**
 * 
 * @brief record thread load operations
 *
 **/
void GS::record_atomic_load(AO *oper) {
    location loc = oper->get_operation_location();
    thread_operation_list inner_map = obj_thread_ld_map[loc];

    inner_map[oper->get_thread()].push_back(oper);
    obj_thread_ld_map[loc] = inner_map;


#ifdef ATOMIC_MODEL_DEBUG
    std::cout << "GS::record_atomic_load/rmwr map size : "
              << inner_map.size() << std::endl;
#endif
}


void GS::record_atomic_rmw(AO *oper) {
    location loc = oper->get_operation_location();
    record_atomic_store(oper);

    thread_operation_list inner_map = obj_thread_rmw_map[loc];
    inner_map[oper->get_thread()].push_back(oper);
}


void GS::record_atomic_fence(AO *oper) {}


/**
 * @brief returns the latest seq_cst store to the location
 *
 **/
AO *GS::get_last_seq_cst_store(location loc) {
    if (obj_last_sc_map.find(loc) == obj_last_sc_map.end()) {
#ifdef ATOMIC_MODEL_DEBUG
        std::cout << "GS::get_last_seq_cst_store : Location Not Found "
                  << std::endl;
#endif
        return nullptr;
    }
    AO *seq_cst_str = obj_last_sc_map[loc];
    return seq_cst_str;
}


/**
 * @brief prints the stores per location per thread
 * 
 **/
void GS::print_thread_stores() {
#ifdef ATOMIC_MODEL_DEBUG
    auto outer_it = obj_thread_str_map.begin();
    for (auto i = outer_it; i != obj_thread_str_map.end(); i++) {
        std::cout << "GS::print_thread_stores : Location : "
                  << i->first << std::endl;

        auto inner_it = outer_it->second.begin();

        std::cout << "GS::print_thread_stores inner_map_len : "
                  << outer_it->second.size() << std::endl;

        for (auto j = inner_it; j != outer_it->second.end(); j++) {
            std::cout << "GS::print_thread_stores : thread : "
                      << j->first->get_id() << std::endl;

            std::cout << "GS::print_thread_stores : number of stores : "
                      << j->second.size() << std::endl;

            for (auto &k : j->second) {
                std::cout << "GS::print_thread_stores : store_seq_no: "
                          << k->get_sequence_number() << std::endl;
            }
        }
    }
#endif
}


/**
 * @brief Print execution trace on assertion failure
 *
 **/
void GS::print_execution_trace() {
#ifdef COYOTE_DEBUG_LOG
    std::cout << "-------------------------------------------------" << std::endl;
    std::cout << "             Execution Trace                     " << std::endl;
    std::cout << "-------------------------------------------------" << std::endl;


    auto trace_iter = this->d_exec_trace.begin();

    while (trace_iter != this->d_exec_trace.end()) {
        AO *oper = *trace_iter;

        std::cout << " seq_no : " << oper->get_sequence_number()
                  << " | location : " << oper->get_operation_location()
                  << " | thrd_id : " << oper->get_thread()->get_id()
                  << " | oper_type : " << operation_type_str[oper->get_operation_type()]
                  << " | oper_order : " << operation_order_str[oper->get_operation_order()]
                  << " | clock_vector : " << oper->get_cv()->print_cv()
                  << std::endl;

        trace_iter++;
    }
    std::cout << "-------------------------------------------------" << std::endl;
#endif
}


void GS::print_thread_map() {
#ifdef ATOMIC_MODEL_DEBUG
    auto tmap_it = thread_id_obj_map.begin();
    for (auto i = tmap_it; i != thread_id_obj_map.end(); i++) {
        std::cout << "GS::print_thread_map : ext_tid : " << i->first
                  << " int_tid : " << i->second->get_id()
                  << std::endl;
    }
#endif
}

/**
 * iteration scavenger
 **/
void GS::clear_state() {
    d_exec_trace.clear();
    d_thread_oper_map.clear();
    thread_id_map.clear();
    thread_id_obj_map.clear();
    obj_str_map.clear();
    obj_thread_oper_map.clear();
    obj_thread_str_map.clear();
    obj_thread_ld_map.clear();
    obj_thread_rmw_map.clear();
    obj_last_sc_map.clear();
}

void GS::print_modification_order() {
#ifdef ATOMIC_MODEL_DEBUG
    std::cout << "-------------------------------" << std::endl;
    std::cout << "Modification Order Graph" << std::endl;
    std::cout << "-------------------------------" << std::endl;
    this->mo_graph.print();
#endif
}
