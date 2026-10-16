// Classroom example: one spinlock protects a two-field configuration.

#include "types.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#ifdef LAB_LOCK

static struct {
  struct spinlock lock;
  int lower;
  int upper;
} example_config;

static int initialized;

uint64
sys_rwexample(void)
{
  int role;
  argint(0, &role);

  // The parent initializes before forking; run one instance at a time.
  if(role == -1){
    if(!initialized){
      initlock(&example_config.lock, "rwexample");
      initialized = 1;
    }
    example_config.lower = 0;
    example_config.upper = 10;
    return 0;
  }

  if(role < 0 || role >= 4)
    return -1;

  for(int i = 0; i < 20000; i++){
    if(role == 2){
      acquire(&example_config.lock);
      if(i % 2 == 0){
        example_config.lower = 0;
        example_config.upper = 10;
      } else {
        example_config.lower = 20;
        example_config.upper = 30;
      }
      release(&example_config.lock);
    } else {
      int lower, upper;
      acquire(&example_config.lock);
      lower = example_config.lower;
      upper = example_config.upper;
      release(&example_config.lock);

      int valid = (lower == 0 && upper == 10) ||
                  (lower == 20 && upper == 30);
      if(!valid)
        return -1;
    }
  }
  return 0;
}

#endif
