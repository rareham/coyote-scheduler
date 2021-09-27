#include <iostream>
#include <thread>

#include "coyote/atomicoperations/atomicoperations.h"
#include "test.h"

using namespace coyote;

constexpr auto WORK_THREAD_1_ID = 2;
constexpr auto WORK_THREAD_2_ID = 3;
constexpr auto WORK_THREAD_3_ID = 4;

coyote::Scheduler *scheduler;

atomic<int> mutex;
atomic<int> acquire_count;
atomic<int> release_count;

atomic<int> a;
atomic<int> b;

bool assert_failed = false;

bool acquire_lock() {
    int release_status = 0;
    int acquire_status = 1;
    if (mutex.compare_exchange_strong(release_status,
                                      acquire_status,
                                      std::memory_order_relaxed)) {
        atomic_fetch_add_explicit(&acquire_count, 1, std::memory_order_relaxed);
        return true;
    }

    return false;
}

bool release_lock() {
    int release_status = 0;
    int acquire_status = 1;
    if (mutex.compare_exchange_strong(acquire_status,
                                      release_status,
                                      std::memory_order_relaxed)) {
        atomic_fetch_add_explicit(&release_count, 1, std::memory_order_relaxed);
        return true;
    }

    return false;
}

static void fn1() {
    scheduler->start_operation(WORK_THREAD_1_ID);
    if (acquire_lock()) {
        int tempa = a.load(std::memory_order_relaxed);
        int tempb = b.load(std::memory_order_relaxed);
        a.store(tempb, std::memory_order_relaxed);
        b.store(tempa, std::memory_order_relaxed);

        release_lock();
    }
    scheduler->complete_operation(WORK_THREAD_1_ID);
}

static void fn2() {
    scheduler->start_operation(WORK_THREAD_2_ID);
    int tempa = a.load(std::memory_order_relaxed);
    int tempb = b.load(std::memory_order_relaxed);
    if (tempa == tempb) {
        int rc = release_count.load(std::memory_order_relaxed);
        int ac = acquire_count.load(std::memory_order_relaxed);

        if (!(rc != 0 || ac != 0)) {
            assert_failed = true;
        }
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

        for (int i = 0; i < 1000; i++) {
            RECORD_PARENT(scheduler->scheduled_operation_id(), scheduler->scheduled_operation_id())
            atomic_init(&mutex, 0);
            atomic_init(&acquire_count, 0);
            atomic_init(&release_count, 0);

            atomic_init(&a, 1);
            atomic_init(&b, 2);

            run_iteration();
            reinitialise_global_state();
        }
        ASSERT_FAIL_CHECK
        delete global_state;
        delete scheduler;
    } catch (std::string error) {
        std::cout << "[test] failed: " << error << std::endl;
        return 1;
    }

    std::cout << "[test] done in " << total_time(start_time) << "ms." << std::endl;
    return 0;
}
