#include <iostream>
#include <thread>

#include "coyote/atomicoperations/atomicoperations.h"
#include "test.h"

using namespace coyote;

coyote::Scheduler *scheduler;

#define N 10
#define M 3

bool assert_failed = false;

atomic<int> x;

void *write(int id) {
    scheduler->start_operation(id);
    x.store(1, std::memory_order_relaxed);
    scheduler->complete_operation(id);
    return nullptr;
}

void *read(int id) {
    scheduler->start_operation(id);
    x.load(std::memory_order_acquire);
    scheduler->complete_operation(id);
    return nullptr;
}

void run_iteration() {
    scheduler->attach();

    std::thread t_write[N], t_read[M];

    for (int i = 0; i < M; i++) {
        int tid = i + 1;
        scheduler->create_operation(M + tid);
        RECORD_PARENT(M + tid, scheduler->scheduled_operation_id())
        t_read[i] = std::thread(&read, M + tid);
    }

    for (int i = 0; i < N; i++) {
        int tid = i + 1;
        scheduler->create_operation(N + M + tid);
        RECORD_PARENT(N + M + tid, scheduler->scheduled_operation_id())
        t_write[i] = std::thread(&write, N + M + tid);
    }

    scheduler->schedule_next();

    for (int i = 0; i < N; i++) {
        int tid = i + 1;
        scheduler->join_operation(M + N + tid);
        t_write[i].join();
    }

    for (int i = 0; i < M; i++) {
        int tid = i + 1;
        scheduler->join_operation(M + tid);
        t_read[i].join();
    }

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
            atomic_init(&x, 1);

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
