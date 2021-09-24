#include <thread>
#include <iostream>

#include "test.h"
#include "coyote/atomicoperations/atomicoperations.h"

using namespace coyote;

constexpr auto WORK_THREAD_1_ID = 2;
constexpr auto WORK_THREAD_2_ID = 3;
constexpr auto WORK_THREAD_3_ID = 4;
constexpr auto WORK_THREAD_4_ID = 5;
constexpr auto WORK_THREAD_5_ID = 6;
constexpr auto WORK_THREAD_6_ID = 7;
constexpr auto WORK_THREAD_7_ID = 8;
constexpr auto WORK_THREAD_8_ID = 9;

coyote::Scheduler *scheduler;

atomic<int> x1;
atomic<int> x2;
atomic<int> x3;
atomic<int> x4;
atomic<int> x5;
atomic<int> x6;
atomic<int> x7;

static void a()
{
  scheduler->start_operation(WORK_THREAD_1_ID);
  atomic_store_explicit(&x1, 1,operation_order::seq_cst);
  scheduler->complete_operation(WORK_THREAD_1_ID);
}

static void b()
{
  scheduler->start_operation(WORK_THREAD_2_ID);
  atomic_load_explicit(&x1, operation_order::seq_cst);
  atomic_store_explicit(&x2, 1,operation_order::seq_cst);	
  scheduler->complete_operation(WORK_THREAD_2_ID);
}

static void c()
{
  scheduler->start_operation(WORK_THREAD_3_ID);
  atomic_load_explicit(&x2, operation_order::seq_cst);
  atomic_store_explicit(&x3, 1,operation_order::seq_cst);
  scheduler->complete_operation(WORK_THREAD_3_ID);
}

static void d()
{
  scheduler->start_operation(WORK_THREAD_4_ID);
  atomic_load_explicit(&x3, operation_order::seq_cst);
  atomic_store_explicit(&x4, 1,operation_order::seq_cst);
  scheduler->complete_operation(WORK_THREAD_4_ID);
}

static void e()
{
  scheduler->start_operation(WORK_THREAD_5_ID);
  atomic_load_explicit(&x4, operation_order::seq_cst);
  atomic_store_explicit(&x5, 1,operation_order::seq_cst);
  scheduler->complete_operation(WORK_THREAD_5_ID);
}

static void f()
{
  scheduler->start_operation(WORK_THREAD_6_ID);
  atomic_load_explicit(&x5, operation_order::seq_cst);
  atomic_store_explicit(&x6, 1,operation_order::seq_cst);
  scheduler->complete_operation(WORK_THREAD_6_ID);
}

static void g()
{
  scheduler->start_operation(WORK_THREAD_7_ID);
  atomic_load_explicit(&x6, operation_order::seq_cst);
  atomic_store_explicit(&x7, 1,operation_order::seq_cst);
  scheduler->complete_operation(WORK_THREAD_7_ID);
}
static void h()
{
  scheduler->start_operation(WORK_THREAD_8_ID);
  atomic_load_explicit(&x7, operation_order::seq_cst);
  atomic_load_explicit(&x1, operation_order::seq_cst);
  scheduler->complete_operation(WORK_THREAD_8_ID);
}

void run_iteration()
{
  scheduler->attach();
  
  scheduler->create_operation(WORK_THREAD_1_ID);
  std::thread t1(a);

  scheduler->create_operation(WORK_THREAD_2_ID);
  std::thread t2(b);

  scheduler->create_operation(WORK_THREAD_3_ID);
  std::thread t3(c);

  scheduler->create_operation(WORK_THREAD_4_ID);
  std::thread t4(d);

  scheduler->create_operation(WORK_THREAD_5_ID);
  std::thread t5(e);

  scheduler->create_operation(WORK_THREAD_6_ID);
  std::thread t6(f);

  scheduler->create_operation(WORK_THREAD_7_ID);
  std::thread t7(g);

  scheduler->create_operation(WORK_THREAD_8_ID);
  std::thread t8(h);

  scheduler->join_operation(WORK_THREAD_1_ID);
  scheduler->join_operation(WORK_THREAD_2_ID);
  scheduler->join_operation(WORK_THREAD_3_ID);
  scheduler->join_operation(WORK_THREAD_4_ID);
  scheduler->join_operation(WORK_THREAD_5_ID);
  scheduler->join_operation(WORK_THREAD_6_ID);
  scheduler->join_operation(WORK_THREAD_7_ID);
  scheduler->join_operation(WORK_THREAD_8_ID);

  t1.join();
  t2.join();
  t3.join();
  t4.join();
  t5.join();
  t6.join();
  t7.join();
  t8.join();
	
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
	  atomic_init(&x1, 0);
	  atomic_init(&x2, 0);
	  atomic_init(&x3, 0);
	  atomic_init(&x4, 0);
	  atomic_init(&x5, 0);
	  atomic_init(&x6, 0);
	  atomic_init(&x7, 0);
	
	  run_iteration();
	  
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
