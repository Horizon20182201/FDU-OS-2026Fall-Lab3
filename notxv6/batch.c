#include "batch.h"
#include <pthread.h>
/* Add fields and synchronization objects as needed.
 * Static integer fields start at zero. Each test uses a fresh process.
 */
enum task_kind { DATA, CONFIG };

int batch_ready(void);
void before_process(enum task_kind kind);
void after_process(void);

static int dthread = 2;
static int cthread = 1;

static struct {
  pthread_mutex_t mutex;
  pthread_cond_t changed;
  int dthread;
  int cthread;
  int finished;
} state = {
  .mutex = PTHREAD_MUTEX_INITIALIZER,
  .changed = PTHREAD_COND_INITIALIZER,
};
void data_task(void)
{
  /* TODO: join a legal batch, call process_data(), and finish this task. */
  before_process(DATA);
  process_data();
  after_process();
}
void config_task(void)
{
  /* TODO: join a legal batch, call process_config(), and finish this task. */
  before_process(CONFIG);
  process_config();
  after_process();
}

int batch_ready(void)
{
  return state.dthread == dthread && state.cthread == cthread;
}

void before_process(enum task_kind kind)
{
  pthread_mutex_lock(&state.mutex);

  if (kind == DATA) {
    while (state.dthread == dthread) pthread_cond_wait(&state.changed, &state.mutex);
    state.dthread++;
  } else if (kind == CONFIG) {
    while (state.cthread == cthread) pthread_cond_wait(&state.changed, &state.mutex);
    state.cthread++;
  }

  if (batch_ready()) {
    pthread_cond_broadcast(&state.changed);
  } else {
    while (!batch_ready()) {
      pthread_cond_wait(&state.changed, &state.mutex);
    }
  }
  pthread_mutex_unlock(&state.mutex);
}

void after_process(void)
{
  pthread_mutex_lock(&state.mutex);
  state.finished++;
  if (state.finished == 3) {
    state.dthread = 0;
    state.cthread = 0;
    state.finished = 0;
    pthread_cond_broadcast(&state.changed);
  }
  pthread_mutex_unlock(&state.mutex);
}