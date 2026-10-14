// Classroom example: protect a two-field configuration with one spinlock.
//
// This is complete example code, not a homework TODO. Three readers and one
// writer run concurrently. A reader must observe either (0, 10) or (20, 30),
// never a mixed pair. The later rwspinlock task preserves this correctness
// while allowing readers to overlap.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "defs.h"

#ifdef LAB_LOCK

static struct {
  struct spinlock lock;
  int lower;
  int upper;
} example_config;

static uint example_barrier;
static uint example_errors;
static uint example_initialized;

static void
example_wait(uint round)
{
  __atomic_fetch_add(&example_barrier, 1, __ATOMIC_ACQ_REL);
  while(__atomic_load_n(&example_barrier, __ATOMIC_ACQUIRE) < 4 * round)
    ;
}

static int
valid_snapshot(int lower, int upper)
{
  return (lower == 0 && upper == 10) ||
         (lower == 20 && upper == 30);
}

uint64
sys_rwexample(void)
{
  int role;
  argint(0, &role);

  // The parent invokes rwexample(-1) before forking workers. This makes the
  // example safe to run more than once during the same xv6 boot.
  if(role == -1){
    if(__atomic_exchange_n(&example_initialized, 1, __ATOMIC_ACQ_REL) == 0)
      initlock(&example_config.lock, "rwexample");
    example_config.lower = 0;
    example_config.upper = 10;
    __atomic_store_n(&example_barrier, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&example_errors, 0, __ATOMIC_RELEASE);
    return 0;
  }

  if(role < 0 || role >= 4)
    return -1;
  example_wait(1);

  if(role == 2){
    for(int i = 0; i < 20000; i++){
      acquire(&example_config.lock);
      if(i & 1){
        example_config.lower = 0;
        example_config.upper = 10;
      } else {
        example_config.lower = 20;
        example_config.upper = 30;
      }
      release(&example_config.lock);
    }
  } else {
    for(int i = 0; i < 20000; i++){
      int lower, upper;
      acquire(&example_config.lock);
      lower = example_config.lower;
      upper = example_config.upper;
      release(&example_config.lock);
      if(!valid_snapshot(lower, upper))
        __atomic_fetch_add(&example_errors, 1, __ATOMIC_RELAXED);
    }
  }

  example_wait(2);
  return __atomic_load_n(&example_errors, __ATOMIC_ACQUIRE) == 0 ? 0 : -1;
}

#endif
