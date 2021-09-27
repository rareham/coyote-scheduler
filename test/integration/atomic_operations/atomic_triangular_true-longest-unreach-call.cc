#include <iostream>
#include <thread>

#include "coyote/atomicoperations/atomicoperations.h"
#include "test.h"

constexpr auto WORK_THREAD_1_ID = 2;
constexpr auto WORK_THREAD_2_ID = 3;
constexpr auto WORK_THREAD_3_ID = 4;

bool assert_failed = false;

using namespace coyote;

Scheduler *scheduler;

atomic<int> i;
atomic<int> j;

#define NUM 20
#define LIMIT (2 * NUM + 6)


static void fn1() {
    scheduler->start_operation(WORK_THREAD_1_ID);
    for (int k = 0; k < NUM; k++) {
        i.store(j.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
    }
    scheduler->complete_operation(WORK_THREAD_1_ID);
}


static void fn2() {
    scheduler->start_operation(WORK_THREAD_2_ID);
    for (int k = 0; k < NUM; k++) {
        j.store(i.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
    }
    scheduler->complete_operation(WORK_THREAD_2_ID);
}


void run_iteration() {
    scheduler->attach();

    scheduler->create_operation(WORK_THREAD_1_ID);
    RECORD_PARENT(WORK_THREAD_1_ID, scheduler->scheduled_operation_id())
    std::thread t1(fn1);

    scheduler->create_operation(WORK_THREAD_2_ID);
    RECORD_PARENT(WORK_THREAD_2_ID, scheduler->scheduled_operation_id())
    std::thread t2(fn2);

    scheduler->schedule_next();

    scheduler->join_operation(WORK_THREAD_1_ID);
    scheduler->join_operation(WORK_THREAD_2_ID);

    t1.join();
    t2.join();

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
            atomic_init(&i, 3);
            atomic_init(&j, 6);

            run_iteration();
            if (i.load(std::memory_order_relaxed) > LIMIT || j.load(std::memory_order_relaxed) > LIMIT) {
                assert_failed = true;
            }
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
