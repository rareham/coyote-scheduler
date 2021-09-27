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

atomic<int> x;
int y;

static void fn1() {
    scheduler->start_operation(WORK_THREAD_1_ID);
    y = 1;
    x.store(1, std::memory_order_release);
    scheduler->complete_operation(WORK_THREAD_1_ID);
}

static void fn2() {
    scheduler->start_operation(WORK_THREAD_2_ID);
    if (x.load(std::memory_order_acquire) == 1) {
        int k = y;
        atomic_fetch_add_explicit(&x, k, std::memory_order_release);
    }
    scheduler->complete_operation(WORK_THREAD_2_ID);
}

static void fn3() {
    scheduler->start_operation(WORK_THREAD_3_ID);
    if (x.load(std::memory_order_acquire) == 2)
        int temp2 = y;
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
    std::thread t3(fn3);

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

        for (int i = 0; i < 1000; i++) {
            RECORD_PARENT(scheduler->scheduled_operation_id(), scheduler->scheduled_operation_id())
            atomic_init(&x, 0);
            y = 0;

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
