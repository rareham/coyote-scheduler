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

atomic<int> x;
atomic<int> y;


static void a()
{
  scheduler->start_operation(WORK_THREAD_1_ID);
  int tempx = x.load(operation_order::relaxed);
  std::cout << "tempx : " << tempx << std::endl;
  y.store(1, operation_order::relaxed);
  scheduler->complete_operation(WORK_THREAD_1_ID);
}

static void b()
{
  scheduler->start_operation(WORK_THREAD_2_ID);
  int tempy = y.load(operation_order::relaxed);
  std::cout << "tempy : " << tempy << std::endl;
  x.store(1, operation_order::relaxed);
  scheduler->complete_operation(WORK_THREAD_2_ID);
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
