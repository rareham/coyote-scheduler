#include <iostream>
#include <thread>

#include "test.h"
#include "coyote/atomicoperations/atomicoperations.h"

using namespace coyote;

constexpr auto WORK_THREAD_1_ID = 2;
constexpr auto WORK_THREAD_2_ID = 3;
constexpr auto WORK_THREAD_3_ID = 4;
constexpr auto WORK_THREAD_4_ID = 5;

Scheduler* scheduler;

bool assert_failed = false;

atomic<int> flag1;
atomic<int> flag2;
atomic<int> turn;
atomic<int> x;
atomic<int> fenc;

#define LOOP 5

static void fn1()
{
  scheduler->start_operation(WORK_THREAD_1_ID);
  int ok1, ok2;
  flag1.store(1, std::memory_order_release);
  atomic_fetch_add_explicit(&fenc, 0, std::memory_order_acq_rel);
  
  ok1 = 0;
  for (int i = 0; i < LOOP; i++) {
    if (flag2.load(std::memory_order_acquire)) {
      if( turn.load(std::memory_order_acquire) != 0) {
	flag1.store(0, std::memory_order_release);
	ok2 = 0;
	for(int j = 0; j < LOOP; j++)
	  {
	    if (turn.load(std::memory_order_acquire) == 0) {
	      ok2 = 1;
	      break;
	    };
	  }
	if (ok2 == 0)
	  {
	    scheduler->complete_operation(WORK_THREAD_1_ID);
	    return;
	  }
	flag1.store(1, std::memory_order_release);
	atomic_fetch_add_explicit(&fenc, 0, std::memory_order_acq_rel);
      }
    }
    else {
      ok1 = 1;
      break;
    }
  }
  if (ok1 == 0) {
    scheduler->complete_operation(WORK_THREAD_1_ID);
    return;
  }

  // begin: critical section
    x.store(0,std::memory_order_relaxed);
    int tempx = x.load(std::memory_order_relaxed);
    if ((tempx == 0))
      {
	assert_failed = true;
      }
    // end: critical section
    turn.store(1, std::memory_order_release);
    flag1.store(0, std::memory_order_release);
    scheduler->complete_operation(WORK_THREAD_1_ID);
}

static void fn2()
{
  scheduler->start_operation(WORK_THREAD_2_ID);
  int ok1, ok2;
  flag2.store(1, std::memory_order_release);
  atomic_fetch_add_explicit(&fenc, 0, std::memory_order_acq_rel);

  ok1 = 0;
  for (int i = 0; i < LOOP; i++) {
    if (flag1.load(std::memory_order_acquire)) {
      if( turn.load(std::memory_order_acquire) != 1) {
	flag2.store(0, std::memory_order_release);
	ok2 = 0;
	for(int j = 0; j < LOOP; j++)
	  {
	    if (turn.load(std::memory_order_acquire) == 1) {
	      ok2 = 1;
	      break;
	    };
	  }
	if (ok2 == 0)
	  {
	    scheduler->complete_operation(WORK_THREAD_2_ID);
	    return;
	  }
	flag2.store(1, std::memory_order_release);
	atomic_fetch_add_explicit(&fenc, 0, std::memory_order_acq_rel);
      }
    }
    else {
      ok1 = 1;
      break;
    }
  }
  if (ok1 == 0)
    {
      scheduler->complete_operation(WORK_THREAD_2_ID);
      return;
    }
  
  // begin: critical section
  x.store(1, std::memory_order_relaxed);
  int tempx = x.load(std::memory_order_relaxed);
  if ((tempx == 1))
    {
      assert_failed = true;
    }
  // end: critical section
  turn.store(0, std::memory_order_release);
  flag2.store(0, std::memory_order_release);
  scheduler->complete_operation(WORK_THREAD_2_ID);
}

void run_iteration()
{
  scheduler->attach();

  scheduler->create_operation(WORK_THREAD_1_ID);
  std::thread t1(fn1);

  scheduler->create_operation(WORK_THREAD_2_ID);
  std::thread t2(fn2);

  scheduler->schedule_next();
			   
  scheduler->join_operation(WORK_THREAD_1_ID);
  scheduler->join_operation(WORK_THREAD_2_ID);

  t1.join();
  t2.join();
	
  scheduler->detach();
  assert(scheduler->error_code(), ErrorCode::Success);
}

int main(int argc, char **argv)
{
  std::cout << "[test] started." << std::endl;
  auto start_time = std::chrono::steady_clock::now();
  
  try
    {
      scheduler = new Scheduler();
      
      initialise_global_state(scheduler);
      
      for (int i = 0; i < 1000; i++)
	{
	  atomic_init(&flag1, 0);
	  atomic_init(&flag2, 0);
	  atomic_init(&turn, 0);
	  atomic_init(&x, 0);
	  atomic_init(&fenc, 0);
	  
	  run_iteration();
	  ASSERT_CHECK
	  reinitialise_global_state();
	}
      delete global_state;
      delete scheduler;
    }
  catch (std::string error)
    {
      std::cout << "[test] failed: " << error << std::endl;
      return 1;
    }
  
  std::cout << "[test] done in " << total_time(start_time) << "ms." << std::endl;
  return 0;
}
