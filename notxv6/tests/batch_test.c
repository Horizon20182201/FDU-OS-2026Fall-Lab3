#include "batch.h"
#include "common.h"

enum { MAX_TASKS = 240 };
static struct {
  pthread_mutex_t mutex;
  pthread_cond_t changed;
  int entered, entered_data, entered_config, callbacks, returned;
  int started, finished, data, config, groups;
  int hold_first, held, release;
  int seen[MAX_TASKS];
} test = {
  .mutex = PTHREAD_MUTEX_INITIALIZER,
  .changed = PTHREAD_COND_INITIALIZER,
};
struct task { int id, config; };
static _Thread_local struct task *current;

static void process(int config)
{
  CHECK(current != NULL, "callback must run on the submitting worker thread");
  CHECK(current->config == config, "wrong callback for task type");
  P(pthread_mutex_lock(&test.mutex));
  CHECK(test.seen[current->id]++ == 0, "a task called its callback twice");
  CHECK(test.started < 3, "next batch started before current batch finished");
  if (test.started == 0) {
    CHECK(test.entered_data >= 2 * (test.groups + 1) &&
          test.entered_config >= test.groups + 1,
          "processing started without enough tasks for this batch");
  }
  int group = test.groups;
  test.started++;
  test.callbacks++;
  if (config) test.config++;
  else test.data++;
  CHECK(test.data <= 2 && test.config <= 1, "batch does not have a 2D:1C quota");
  if (test.started == 3) {
    CHECK(test.data == 2 && test.config == 1, "invalid batch composition");
    P(pthread_cond_broadcast(&test.changed));
  }
  /* Deliberately require all three callbacks to be able to enter together.
   * A solution holding its scheduler lock throughout processing will time out.
   */
  while (test.started < 3) P(pthread_cond_wait(&test.changed, &test.mutex));
  if (test.hold_first && group == 0 && config) {
    test.held = 1;
    P(pthread_cond_broadcast(&test.changed));
    while (!test.release) P(pthread_cond_wait(&test.changed, &test.mutex));
  }
  P(pthread_mutex_unlock(&test.mutex));
  pause_ms((current->id * 7 % 3) + 1);
  P(pthread_mutex_lock(&test.mutex));
  CHECK(group == test.groups, "batch state advanced before all tasks completed");
  test.finished++;
  if (test.finished == 3) {
    test.groups++;
    test.started = test.finished = test.data = test.config = 0;
    P(pthread_cond_broadcast(&test.changed));
  }
  P(pthread_mutex_unlock(&test.mutex));
}
void process_data(void) { process(0); }
void process_config(void) { process(1); }

static void *worker(void *arg)
{
  current = arg;
  P(pthread_mutex_lock(&test.mutex));
  test.entered++;
  if (current->config) test.entered_config++;
  else test.entered_data++;
  P(pthread_cond_broadcast(&test.changed));
  P(pthread_mutex_unlock(&test.mutex));
  if (current->config) config_task();
  else data_task();
  P(pthread_mutex_lock(&test.mutex));
  CHECK(test.seen[current->id] == 1, "task returned without processing exactly once");
  test.returned++;
  P(pthread_mutex_unlock(&test.mutex));
  return NULL;
}

static pthread_t threads[MAX_TASKS];
static struct task tasks[MAX_TASKS];
static int launched;
static void launch(int config)
{
  CHECK(launched < MAX_TASKS, "test task limit");
  tasks[launched] = (struct task){launched, config};
  P(pthread_create(&threads[launched], NULL, worker, &tasks[launched]));
  launched++;
}
static void wait_entered(void)
{
  P(pthread_mutex_lock(&test.mutex));
  while (test.entered < launched) P(pthread_cond_wait(&test.changed, &test.mutex));
  P(pthread_mutex_unlock(&test.mutex));
}
static void expect_waiting(int completed_groups)
{
  wait_entered();
  double before = cpu_seconds();
  pause_ms(300);
  double used = cpu_seconds() - before;
  P(pthread_mutex_lock(&test.mutex));
  CHECK(test.callbacks == 3 * completed_groups,
        "processing started before a full 2D+1C batch was available");
  P(pthread_mutex_unlock(&test.mutex));
  CHECK(used < 0.12, "waiting consumed CPU: busy waiting is not allowed");
}
static void wait_groups(int groups)
{
  P(pthread_mutex_lock(&test.mutex));
  while (test.groups < groups) P(pthread_cond_wait(&test.changed, &test.mutex));
  P(pthread_mutex_unlock(&test.mutex));
}
static void join_all(int groups)
{
  for (int i = 0; i < launched; i++) P(pthread_join(threads[i], NULL));
  CHECK(test.returned == launched, "some tasks did not return");
  CHECK(test.callbacks == launched, "missing or extra callback");
  CHECK(test.groups == groups, "wrong number of completed batches");
  CHECK(test.started == 0 && test.finished == 0, "incomplete final batch");
}
static void shuffled(int groups, unsigned seed)
{
  int types[MAX_TASKS];
  int n = 3 * groups;
  for (int i = 0; i < n; i++) types[i] = i < groups;
  for (int i = n - 1; i > 0; i--) {
    seed = seed * 1664525u + 1013904223u;
    int j = (int)(seed % (unsigned)(i + 1));
    int temp = types[i]; types[i] = types[j]; types[j] = temp;
  }
  for (int i = 0; i < n; i++) launch(types[i]);
}
int main(int argc, char **argv)
{
  CHECK(argc >= 2 && argc <= 3, "usage: ./batch_test CASE [SEED]");
  const char *name = argv[1];
  unsigned seed = argc == 3 ? (unsigned)strtoul(argv[2], NULL, 10) : 2026;
  if (!strcmp(name, "data-first")) {
    launch(0); launch(0); expect_waiting(0); launch(1); join_all(1);
  } else if (!strcmp(name, "config-first")) {
    for (int i = 0; i < 4; i++) launch(1);
    expect_waiting(0);
    for (int i = 0; i < 8; i++) launch(0);
    join_all(4);
  } else if (!strcmp(name, "incomplete")) {
    launch(0); launch(1); expect_waiting(0); launch(0); join_all(1);
  } else if (!strcmp(name, "slow-batch")) {
    test.hold_first = 1;
    launch(0); launch(0); launch(1);
    P(pthread_mutex_lock(&test.mutex));
    while (!test.held) P(pthread_cond_wait(&test.changed, &test.mutex));
    P(pthread_mutex_unlock(&test.mutex));
    for (int i = 0; i < 3; i++) { launch(0); launch(1); launch(0); }
    wait_entered();
    pause_ms(100);
    P(pthread_mutex_lock(&test.mutex));
    CHECK(test.callbacks == 3 && test.groups == 0,
      "next batch entered while first batch was held");
    test.release = 1;
    P(pthread_cond_broadcast(&test.changed));
    P(pthread_mutex_unlock(&test.mutex));
    join_all(4);
  } else if (!strcmp(name, "mixed")) {
    /* Check readiness again after rollover, before launching the missing type.
     * The callback rendezvous must not hide a missing admission barrier.
     */
    shuffled(1, seed);
    wait_groups(1);
    launch(0); launch(0); expect_waiting(1); launch(1);
    wait_groups(2);
    launch(0); launch(1); expect_waiting(2); launch(0);
    wait_groups(3);
    shuffled(5, seed + 1);
    join_all(8);
  } else if (!strcmp(name, "stress")) {
    shuffled(60, seed); join_all(60);
  } else CHECK(0, "unknown test case");
  passed(name);
  return 0;
}
