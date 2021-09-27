#include <iostream>
#include <iterator>
#include <thread>

#include "coyote/atomicoperations/atomicoperations.h"
#include "test.h"


using namespace coyote;

constexpr auto MAIN_THREAD_ID = 1;
constexpr auto WORK_THREAD_1_ID = 2;
constexpr auto WORK_THREAD_2_ID = 3;
constexpr auto LOCK_ID = 1;

#ifdef COYOTE_DEBUG_LOG
#undef COYOTE_DEBUG_LOG
#endif


atomic<int> x;
atomic<int> y;

bool assert_failed = false;

Scheduler *scheduler;

void work_1() {
    scheduler->start_operation(WORK_THREAD_1_ID);
    atomic_store(&x, 1, std::memory_order_relaxed);
    atomic_store(&y, 2, std::memory_order_relaxed);
    scheduler->schedule_next();
    scheduler->complete_operation(WORK_THREAD_1_ID);
}

void work_2() {
    scheduler->start_operation(WORK_THREAD_2_ID);
    value loadx = atomic_load(&x, std::memory_order_relaxed);
    std::cout << "work_2 :: loadx : " << loadx << std::endl;
    scheduler->schedule_next();
    value loady = atomic_load(&y, std::memory_order_relaxed);
    std::cout << "work_2 :: loady : " << loady << std::endl;
    scheduler->schedule_next();
    scheduler->complete_operation(WORK_THREAD_2_ID);
}

void run_iteration() {
    scheduler->attach();

    scheduler->create_operation(WORK_THREAD_1_ID);
    RECORD_PARENT(WORK_THREAD_1_ID, scheduler->scheduled_operation_id())
    std::thread t1(work_1);

    scheduler->create_operation(WORK_THREAD_2_ID);
    RECORD_PARENT(WORK_THREAD_2_ID, scheduler->scheduled_operation_id())
    std::thread t2(work_2);

    scheduler->schedule_next();

    scheduler->join_operation(WORK_THREAD_1_ID);
    t1.join();

    scheduler->join_operation(WORK_THREAD_2_ID);
    t2.join();

    scheduler->detach();
    //assert((scheduler->error_code(), ErrorCode::Success);
}

int main() {
    std::cout << "[test] started." << std::endl;
    auto start_time = std::chrono::steady_clock::now();

    try {
        scheduler = new Scheduler();
        initialise_global_state(scheduler);

        for (int i = 0; i < 1000; i++) {
            RECORD_PARENT(scheduler->scheduled_operation_id(), scheduler->scheduled_operation_id())
            atomic_init(&x, 0);
            atomic_init(&y, 0);

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
