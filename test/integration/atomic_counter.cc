//#include <atomic>
#include <iostream>
#include <thread>


#include "test.h"
#include "coyote/operations/atomicoperations.h"

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

Scheduler* scheduler;

static void increment()
{
  scheduler->start_operation(WORK_THREAD_1_ID);
  for (int i = 0; i < ROUND; i++)
    {
      //atomic_fetch_add_explicit(&x, 1, memory_order_relaxed);
      atomic_fetch_add(&x, 1, operation_order::relaxed);
    }
  scheduler->complete_operation(WORK_THREAD_1_ID);
}

static void decrement()
{
  scheduler->start_operation(WORK_THREAD_2_ID);
  for (int i = 0; i < ROUND; i++)
    {
      //atomic_fetch_sub_explicit(&y, 1, memory_order_relaxed);
      atomic_fetch_sub(&y, 1, operation_order::relaxed);
    }
  scheduler->complete_operation(WORK_THREAD_2_ID);
}

static void read()
{
  scheduler->start_operation(WORK_THREAD_3_ID);
  //  a = x.load(memory_order_relaxed);
  a = atomic_load(&x, operation_order::relaxed);
  std::cout << "read:: a: " << a << std::endl;
  scheduler->schedule_next();
  //  b = y.load(memory_order_relaxed);
  b = atomic_load(&y, operation_order::relaxed);
  std::cout << "read:: b: " << b << std::endl;
  scheduler->schedule_next();
  scheduler->complete_operation(WORK_THREAD_3_ID);
}

static void read2()
{
  scheduler->start_operation(WORK_THREAD_4_ID);
  //  c = y.load(memory_order_relaxed);
  c = atomic_load(&y, operation_order::relaxed);
  std::cout << "read:: c: " << c << std::endl;
  scheduler->schedule_next();
  //  d = x.load(memory_order_relaxed);
  d = atomic_load(&x, operation_order::relaxed);
  std::cout << "read:: d: " << d << std::endl;
  scheduler->schedule_next();
  scheduler->complete_operation(WORK_THREAD_4_ID);
}

void run_iteration()
{
  scheduler->attach();

  scheduler->create_operation(WORK_THREAD_1_ID);
  //thrd_create(&t1, (thrd_start_t)&increment, NULL);
  std::thread t1(increment);

  scheduler->create_operation(WORK_THREAD_2_ID);
  //thrd_create(&t2, (thrd_start_t)&decrement, NULL);
  std::thread t2(decrement);

  scheduler->create_operation(WORK_THREAD_3_ID);
  //thrd_create(&t3, (thrd_start_t)&read, NULL);
  std::thread t3(read);

  scheduler->create_operation(WORK_THREAD_4_ID);
  //thrd_create(&t4, (thrd_start_t)&read2, NULL);
  std::thread t4(read2);

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

      for (int i = 0; i < 1000; i++)
	{
	  a = 0;//x
	  b = ROUND;//y
	  c = 0;//y
	  d = ROUND;//x

	  atomic_init(&x, 0);
	  atomic_init(&y, ROUND);
	  
#ifdef COYOTE_DEBUG_LOG
#endif // COYOTE_DEBUG_LOG
	  run_iteration();

      assert(!(a > 0 && b == ROUND && c < ROUND&& d == 0), "Failed Assertion");

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
  
  std::cout << "a= " << a << "b= " << b << "c= "  << c << "d= " << d << std::endl;
 

  std::cout << "[test] done in " << total_time(start_time) << "ms." << std::endl;
  return 0;
}
