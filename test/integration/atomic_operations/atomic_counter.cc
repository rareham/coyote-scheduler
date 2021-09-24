#include <iostream>
#include <thread>

#include "test.h"
#include "coyote/atomicoperations/atomicoperations.h"

#define ROUND 15

using namespace coyote;

constexpr auto MAIN_THREAD_ID = 1;
constexpr auto WORK_THREAD_1_ID = 2;
constexpr auto WORK_THREAD_2_ID = 3;
constexpr auto WORK_THREAD_3_ID = 4;
constexpr auto WORK_THREAD_4_ID = 5;

constexpr auto LOCK_ID = 1;

atomic<int> x;
atomic<int> y;
int a, b, c, d;

bool assert_failed = false;

Scheduler* scheduler;

static void increment()
{
  scheduler->start_operation(WORK_THREAD_1_ID);
  for (int i = 0; i < ROUND; i++)
    {
      atomic_fetch_add_explicit(&x, 1, std::memory_order_relaxed);
    }
  scheduler->complete_operation(WORK_THREAD_1_ID);
}



static void decrement()
{
  scheduler->start_operation(WORK_THREAD_2_ID);
  for (int i = 0; i < ROUND; i++)
    {
      atomic_fetch_sub_explicit(&y, 1, std::memory_order_relaxed);
    }
  scheduler->complete_operation(WORK_THREAD_2_ID);
}



static void read()
{
  scheduler->start_operation(WORK_THREAD_3_ID);
  a = x.load(std::memory_order_relaxed);
  std::cout << "read:: a: " << a << std::endl;
  
  b = y.load(std::memory_order_relaxed);
  std::cout << "read:: b: " << b << std::endl;
  
  scheduler->complete_operation(WORK_THREAD_3_ID);
}



static void read2()
{
  scheduler->start_operation(WORK_THREAD_4_ID);
  c = y.load(std::memory_order_relaxed);
  std::cout << "read:: c: " << c << std::endl;
  
  d = x.load(std::memory_order_relaxed);
  std::cout << "read:: d: " << d << std::endl;
  
  scheduler->complete_operation(WORK_THREAD_4_ID);
}



void run_iteration()
{
  scheduler->attach();

  scheduler->create_operation(WORK_THREAD_1_ID);
  std::thread t1(increment);

  scheduler->create_operation(WORK_THREAD_2_ID);
  std::thread t2(decrement);

  scheduler->create_operation(WORK_THREAD_3_ID);
  std::thread t3(read);

  scheduler->create_operation(WORK_THREAD_4_ID);
  std::thread t4(read2);

  scheduler->schedule_next();
  
  scheduler->join_operation(WORK_THREAD_1_ID);
  scheduler->join_operation(WORK_THREAD_2_ID);
  scheduler->join_operation(WORK_THREAD_3_ID);
  scheduler->join_operation(WORK_THREAD_4_ID);

  t1.join();
  t2.join();
  t3.join();
  t4.join();

  scheduler->create_resource(LOCK_ID);
	
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

      for (int i = 0; i < 100; i++)
	{
	  a = 0;//x
	  b = ROUND;//y
	  c = 0;//y
	  d = ROUND;//x
	  
	  atomic_init(&x, 0);
	  atomic_init(&y, ROUND);
	  
      	  run_iteration();

	  if ((a > 0 && b == ROUND && c < ROUND&& d == 0))
	    {
	      assert_failed = true;
	    }
	  
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
