#include "batch.h"
#include <pthread.h>
/* Add fields and synchronization objects as needed.
 * Static integer fields start at zero. Each test uses a fresh process.
 */
static struct {
  pthread_mutex_t mutex;
  pthread_cond_t changed;
} state = {
  .mutex = PTHREAD_MUTEX_INITIALIZER,
  .changed = PTHREAD_COND_INITIALIZER,
};
void data_task(void)
{
  /* TODO: join a legal batch, call process_data(), and finish this task. */
  (void)&state;
}
void config_task(void)
{
  /* TODO: join a legal batch, call process_config(), and finish this task. */
  (void)&state;
}
