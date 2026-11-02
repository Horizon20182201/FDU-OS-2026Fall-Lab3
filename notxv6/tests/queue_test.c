#include "queue.h"
#include "common.h"

struct action {
  struct queue *q;
  int put, value, result, entered, done;
  pthread_mutex_t mutex;
  pthread_cond_t changed;
};
static void *one_action(void *arg)
{
  struct action *a = arg;
  P(pthread_mutex_lock(&a->mutex));
  a->entered = 1;
  P(pthread_cond_broadcast(&a->changed));
  P(pthread_mutex_unlock(&a->mutex));
  if (a->put) queue_put(a->q, a->value);
  else a->result = queue_get(a->q);
  P(pthread_mutex_lock(&a->mutex));
  a->done = 1;
  P(pthread_cond_broadcast(&a->changed));
  P(pthread_mutex_unlock(&a->mutex));
  return NULL;
}
static void blocking_test(int full)
{
  struct queue q;
  queue_init(&q, 1);
  if (full) queue_put(&q, 11);
  struct action a = {.q = &q, .put = full, .value = 22,
    .mutex = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
  pthread_t worker;
  P(pthread_create(&worker, NULL, one_action, &a));
  P(pthread_mutex_lock(&a.mutex));
  while (!a.entered) P(pthread_cond_wait(&a.changed, &a.mutex));
  P(pthread_mutex_unlock(&a.mutex));
  double before = cpu_seconds();
  pause_ms(300);
  double used = cpu_seconds() - before;
  P(pthread_mutex_lock(&a.mutex));
  CHECK(!a.done, full ? "put returned while queue was full" : "get returned while queue was empty");
  P(pthread_mutex_unlock(&a.mutex));
  CHECK(used < 0.12, "waiting consumed CPU: use condition variables, not busy waiting");
  if (full) CHECK(queue_get(&q) == 11, "first value corrupted");
  else queue_put(&q, 22);
  P(pthread_join(worker, NULL));
  if (full) CHECK(queue_get(&q) == 22, "blocked put lost its value");
  else CHECK(a.result == 22, "blocked get returned wrong value");
  queue_destroy(&q);
  P(pthread_mutex_destroy(&a.mutex));
  P(pthread_cond_destroy(&a.changed));
}
static void fifo_test(void)
{
  struct queue q;
  queue_init(&q, 7);
  for (int round = 0; round < 1000; round++) {
    for (int j = 0; j < 7; j++) queue_put(&q, round * 7 + j);
    for (int j = 0; j < 7; j++)
      CHECK(queue_get(&q) == round * 7 + j, "FIFO order or wraparound is incorrect");
  }
  queue_put(&q, -1);
  queue_put(&q, -12345);
  CHECK(queue_get(&q) == -1, "negative value corrupted");
  CHECK(queue_get(&q) == -12345, "negative value corrupted");
  queue_destroy(&q);
}
struct stress {
  struct queue q;
  int items, producers, consumers, *seen;
  pthread_mutex_t check;
};
struct worker_arg { struct stress *s; int id; };
static void *produce(void *arg)
{
  struct worker_arg *a = arg;
  for (int i = a->id; i < a->s->items; i += a->s->producers) {
    queue_put(&a->s->q, i);
    if (i % 97 == 0) pause_ms(1);
  }
  return NULL;
}
static void *consume(void *arg)
{
  struct worker_arg *a = arg;
  for (int i = a->id; i < a->s->items; i += a->s->consumers) {
    int value = queue_get(&a->s->q);
    CHECK(value >= 0 && value < a->s->items, "invalid item returned");
    P(pthread_mutex_lock(&a->s->check));
    CHECK(a->s->seen[value]++ == 0, "item consumed more than once");
    P(pthread_mutex_unlock(&a->s->check));
  }
  return NULL;
}
static void stress_test(int capacity, int np, int nc)
{
  struct stress s = {.items = 12000, .producers = np, .consumers = nc,
    .check = PTHREAD_MUTEX_INITIALIZER};
  s.seen = calloc((size_t)s.items, sizeof(int));
  CHECK(s.seen != NULL, "calloc");
  queue_init(&s.q, capacity);
  pthread_t threads[16];
  struct worker_arg args[16];
  for (int i = 0; i < np + nc; i++) {
    args[i] = (struct worker_arg){&s, i < np ? i : i - np};
    P(pthread_create(&threads[i], NULL, i < np ? produce : consume, &args[i]));
  }
  for (int i = 0; i < np + nc; i++) P(pthread_join(threads[i], NULL));
  for (int i = 0; i < s.items; i++) CHECK(s.seen[i] == 1, "item missing");
  queue_destroy(&s.q);
  free(s.seen);
  P(pthread_mutex_destroy(&s.check));
}
/* Notify without changing state, to exercise condition rechecking. */
static void wakeup_test(int full)
{
  struct queue q;
  queue_init(&q, 1);
  if (full) queue_put(&q, 11);
  enum { N = 4 };
  pthread_t threads[N];
  struct action a[N];
  for (int i = 0; i < N; i++) {
    memset(&a[i], 0, sizeof(a[i]));
    a[i].q = &q; a[i].put = full; a[i].value = 100 + i;
    P(pthread_mutex_init(&a[i].mutex, NULL));
    P(pthread_cond_init(&a[i].changed, NULL));
    P(pthread_create(&threads[i], NULL, one_action, &a[i]));
  }
  for (int i = 0; i < N; i++) {
    P(pthread_mutex_lock(&a[i].mutex));
    while (!a[i].entered) P(pthread_cond_wait(&a[i].changed, &a[i].mutex));
    P(pthread_mutex_unlock(&a[i].mutex));
  }
  for (int k = 0; k < 15; k++) {
    P(pthread_mutex_lock(&q.mutex));
    P(pthread_cond_broadcast(full ? &q.not_full : &q.not_empty));
    P(pthread_mutex_unlock(&q.mutex));
    pause_ms(5);
    for (int i = 0; i < N; i++) {
      P(pthread_mutex_lock(&a[i].mutex));
      CHECK(!a[i].done, "notification without a state change allowed an operation");
      P(pthread_mutex_unlock(&a[i].mutex));
    }
  }
  if (full) {
    CHECK(queue_get(&q) == 11, "initial item corrupted");
    int seen[N] = {0};
    for (int i = 0; i < N; i++) {
      int value = queue_get(&q);
      CHECK(value >= 100 && value < 100 + N, "invalid producer item");
      CHECK(seen[value - 100]++ == 0, "producer item duplicated");
    }
  } else {
    for (int i = 0; i < N; i++) queue_put(&q, 55);
  }
  for (int i = 0; i < N; i++) {
    P(pthread_join(threads[i], NULL));
    if (!full) CHECK(a[i].result == 55, "consumer received wrong value");
    P(pthread_mutex_destroy(&a[i].mutex));
    P(pthread_cond_destroy(&a[i].changed));
  }
  queue_destroy(&q);
}
int main(int argc, char **argv)
{
  CHECK(argc == 2, "usage: ./queue_test CASE");
  const char *name = argv[1];
  if (!strcmp(name, "fifo")) fifo_test();
  else if (!strcmp(name, "empty")) blocking_test(0);
  else if (!strcmp(name, "full")) blocking_test(1);
  else if (!strcmp(name, "wake-empty")) wakeup_test(0);
  else if (!strcmp(name, "wake-full")) wakeup_test(1);
  else if (!strcmp(name, "spsc")) stress_test(5, 1, 1);
  else if (!strcmp(name, "mpmc")) stress_test(7, 4, 4);
  else if (!strcmp(name, "capacity1")) stress_test(1, 4, 4);
  else if (!strcmp(name, "unbalanced")) stress_test(3, 2, 6);
  else CHECK(0, "unknown test case");
  passed(name);
  return 0;
}
