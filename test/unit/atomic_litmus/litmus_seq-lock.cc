#include <thread>
#include <iostream>

#include "test.h"
#include "coyote/atomicoperations/atomicoperations.h"

using namespace coyote;

constexpr auto WORK_THREAD_1_ID = 2;
constexpr auto WORK_THREAD_2_ID = 3;
constexpr auto WORK_THREAD_3_ID = 4;
constexpr auto WORK_THREAD_4_ID = 5;

coyote::Scheduler *scheduler;

bool assert_failed = false;
atomic<int> x;
atomic<int> y;
atomic<int> z;

static int N = 1;

static void a()
{
  scheduler->start_operation(WORK_THREAD_1_ID);
  for (int i = 0; i < N; i++) {
    x.store(2 * i + 1, operation_order::release);
    y.store(i + 1, operation_order::release);
    z.store(i + 1, operation_order::release);
    x.store(2 * i + 2, operation_order::release);
  }
  scheduler->complete_operation(WORK_THREAD_1_ID);
}

static void b()
{
  int x1, y1, z1, x2;
  
  scheduler->start_operation(WORK_THREAD_2_ID);
  x1 = x.load(operation_order::acquire);
  y1 = y.load(operation_order::acquire);
  z1 = z.load(operation_order::acquire);
  x2 = x.load(operation_order::acquire);
  std::cout << "x: " << x1 << "y: "  << y1
	    << "z: " << z1 << "x: "  << x2
	    << std::endl;

  scheduler->complete_operation(WORK_THREAD_2_ID);
  /* If x1 and x2 are the same, even value, then y1 must equal z1 */
  if (!(x1 != x2 || x1 & 0x1 || y1 == z1))
    {
      assert_failed = true;
    }
}

void run_iteration()
{
  scheduler->attach();
  
  scheduler->create_operation(WORK_THREAD_1_ID);
  std::thread t1(a);

  scheduler->create_operation(WORK_THREAD_2_ID);
  std::thread t2(b);

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
	  atomic_init(&x, 0);
	  atomic_init(&y, 0);
	  atomic_init(&z, 0);
	  
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
