#include <iostream>
#include <thread>

#include "test.h"
#include "coyote/atomicoperations/atomicoperations.h"

#define ROUND 15

using namespace coyote;

constexpr auto WORK_THREAD_1_ID = 2;
constexpr auto WORK_THREAD_2_ID = 3;

bool assert_failed = false;

atomic<int> flag1;
atomic<int> flag2;
atomic<int> turn;
atomic<int> x;

#define LOOP 5

Scheduler* scheduler;

static void t1() {
  int ok1, ok2;
  scheduler->start_operation(WORK_THREAD_1_ID);
  flag1.store(1, std::memory_order_seq_cst);
  ok1 = 0;
  for (int i = 0; i < LOOP; i++) {
    if (flag1.load(std::memory_order_acquire)) {
      
      if (flag2.load(std::memory_order_acquire)) {
	
	if(turn.load(std::memory_order_relaxed) != 0) {
	  
	  flag1.store(0, std::memory_order_seq_cst);
	  ok2 = 0;
	  for(int j = 0; j < LOOP; j++)
	    {
	      if (turn.load(std::memory_order_relaxed) == 0) {
		
		ok2 = 1;
		break;
	      };
	    }
	  if (ok2 == 0)
	    {
	      scheduler->complete_operation(WORK_THREAD_1_ID);
	      return;
	    }
	  flag1.store(1, std::memory_order_seq_cst);
	}
      }
      else {
	ok1 = 1;
	break;
      }
    }
  }
  if (ok1 == 0)
    {
      scheduler->complete_operation(WORK_THREAD_1_ID);
      return;
    }

  // begin: critical section
  x.store(1, std::memory_order_relaxed);
  int loadx = x.load(std::memory_order_relaxed);
  if(!(loadx == 1))
    {
      assert_failed = true;
    }
  // end: critical section
  turn.store(1, std::memory_order_release);
  flag1.store(0, std::memory_order_seq_cst);
  scheduler->complete_operation(WORK_THREAD_1_ID);
}

static void t2() {
  int ok1, ok2;
  scheduler->start_operation(WORK_THREAD_2_ID);
  flag2.store(1, std::memory_order_seq_cst);
  
  ok1 = 0;
  for (int i = 0; i < LOOP; i++)
    {
      if (flag2.load(std::memory_order_acquire))
	{
	  
	  if (flag1.load(std::memory_order_acquire))
	    {
	      
	      if( turn.load(std::memory_order_relaxed) != 1)
		{
		  
		  flag2.store(0, std::memory_order_seq_cst);
		  ok2 = 0;
		  for(int j = 0; j < LOOP; j++)
		    {
		      if (turn.load(std::memory_order_relaxed) == 1)
			{
			  
			  ok2 = 1;
			  break;
			};
		    }
		  if (ok2 == 0)
		    {
		      scheduler->complete_operation(WORK_THREAD_2_ID);
		      return;
		    }
		  flag2.store(1, std::memory_order_seq_cst);
		}
	    }
	  else
	    {
	      ok1 = 1;
	      break;
	    }
	}
    }
  if (ok1 == 0)
    {
      scheduler->complete_operation(WORK_THREAD_2_ID);
      return;
    }
  
  // begin: critical section
  x.store(2, std::memory_order_relaxed);
  int loadx = x.load(std::memory_order_relaxed);
  if(!(loadx == 2))
    {
      assert_failed = true;
    }
  // end: critical section
  turn.store(0, std::memory_order_release);
  flag2.store(0, std::memory_order_seq_cst);
  scheduler->complete_operation(WORK_THREAD_2_ID);
}


void run_iteration()
{
  scheduler->attach();
    
  scheduler->create_operation(WORK_THREAD_1_ID);
  std::thread th1(t1);

  scheduler->create_operation(WORK_THREAD_2_ID);
  std::thread th2(t2);
  
  scheduler->schedule_next();

  scheduler->join_operation(WORK_THREAD_1_ID);
  th1.join();
  
  scheduler->join_operation(WORK_THREAD_2_ID);
  th2.join();
  
  scheduler->detach();
  assert(scheduler->error_code(), ErrorCode::Success);
}




int main()
{
  std::cout << "[test] started." << std::endl;
  auto start_time = std::chrono::steady_clock::now();
  
  try
    {
      scheduler = new Scheduler();
      
      initialise_global_state(scheduler);

      for (int i = 0; i < 100; i++)
        {
	  atomic_init(&flag1, 0);
	  atomic_init(&flag2, 0);
	  atomic_init(&turn, 0);
	  atomic_init(&x, 0);
	  
#ifdef COYOTE_DEBUG_LOG
	  std::cout << "[test] iteration " << i << std::endl;
#endif // COYOTE_DEBUG_LOG
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
