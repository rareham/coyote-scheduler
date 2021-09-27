#include <iostream>
#include <thread>

#include "coyote/atomicoperations/atomicoperations.h"
#include "test.h"

using namespace coyote;

constexpr auto WORK_THREAD_1_ID = 2;
constexpr auto WORK_THREAD_2_ID = 3;
constexpr auto WORK_THREAD_3_ID = 4;

coyote::Scheduler *scheduler;

bool assert_failed = false;

atomic<int> i;
atomic<int> j;
atomic<int> count_i;
atomic<int> count_j;

#define NUM 2
#define FIB_NUM 8

static void fn1() {
    scheduler->start_operation(WORK_THREAD_1_ID);
    int k = 0;

    for (k = 0; k < NUM; k++) {
        int temp_j = j.load(std::memory_order_relaxed);
        int temp_i = i.load(std::memory_order_relaxed);
        i.store(temp_i + temp_j, std::memory_order_relaxed);
        if (temp_j > temp_i) {
            atomic_fetch_add_explicit(&count_j, 1, std::memory_order_acq_rel);
        }
    }
    scheduler->complete_operation(WORK_THREAD_1_ID);
}

static void fn2() {
    scheduler->start_operation(WORK_THREAD_2_ID);
    int k = 0;
    for (k = 0; k < NUM; k++) {
        int temp_i = i.load(std::memory_order_relaxed);
        int temp_j = j.load(std::memory_order_relaxed);
        j.store(temp_j + temp_i, std::memory_order_relaxed);
        if (temp_i > temp_j) {
            atomic_fetch_add_explicit(&count_i, 1, std::memory_order_acq_rel);
        }
    }
    scheduler->complete_operation(WORK_THREAD_2_ID);
}

static void check_assert() {
    scheduler->start_operation(WORK_THREAD_3_ID);
    int c_i = count_i.load(std::memory_order_acquire);
    int c_j = count_j.load(std::memory_order_acquire);
    if ((c_i == NUM and c_j == NUM - 1) or (c_i == NUM - 1 and c_j == NUM)) {
        int temp_i = i.load(std::memory_order_relaxed);
        int temp_j = j.load(std::memory_order_relaxed);

        if (!(temp_i == FIB_NUM || temp_j == FIB_NUM)) {
            assert_failed = true;
        }
    }
    scheduler->complete_operation(WORK_THREAD_3_ID);
}

void run_iteration() {
    scheduler->attach();

    scheduler->create_operation(WORK_THREAD_1_ID);
    RECORD_PARENT(WORK_THREAD_1_ID, scheduler->scheduled_operation_id())
    std::thread t1(fn1);

    scheduler->create_operation(WORK_THREAD_2_ID);
    RECORD_PARENT(WORK_THREAD_2_ID, scheduler->scheduled_operation_id())
    std::thread t2(fn2);

    scheduler->create_operation(WORK_THREAD_3_ID);
    RECORD_PARENT(WORK_THREAD_3_ID, scheduler->scheduled_operation_id())
    std::thread t3(check_assert);

    scheduler->schedule_next();

    scheduler->join_operation(WORK_THREAD_1_ID);
    scheduler->join_operation(WORK_THREAD_2_ID);
    scheduler->join_operation(WORK_THREAD_3_ID);

    t1.join();
    t2.join();
    t3.join();

    scheduler->detach();
    //assert((scheduler->error_code(), ErrorCode::Success);
}


int main(int argc, char **argv) {
    std::cout << "[test] started." << std::endl;
    auto start_time = std::chrono::steady_clock::now();

    try {
        scheduler = new Scheduler();

        initialise_global_state(scheduler);

        for (int x = 0; x < 1000; x++) {
            RECORD_PARENT(scheduler->scheduled_operation_id(), scheduler->scheduled_operation_id())
            atomic_init(&i, 1);
            atomic_init(&j, 1);
            atomic_init(&count_i, 0);
            atomic_init(&count_j, 0);

            run_iteration();
            ASSERT_PASS_CHECK
            reinitialise_global_state();
        }
        delete global_state;
        delete scheduler;
    } catch (std::string error) {
        std::cout << "[test] failed: " << error << std::endl;
        return 1;
    }

    std::cout << "[test] done in " << total_time(start_time) << "ms." << std::endl;
    return 0;
}
