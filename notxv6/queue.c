#include "queue.h"
#include <assert.h>
void queue_init(struct queue *q, int capacity)
{
  assert(capacity >= 1 && capacity <= QUEUE_MAX_CAPACITY);
  q->capacity = capacity;
  q->head = q->tail = q->count = 0;
  assert(pthread_mutex_init(&q->mutex, NULL) == 0);
  assert(pthread_cond_init(&q->not_empty, NULL) == 0);
  assert(pthread_cond_init(&q->not_full, NULL) == 0);
}
void queue_destroy(struct queue *q)
{
  assert(pthread_mutex_destroy(&q->mutex) == 0);
  assert(pthread_cond_destroy(&q->not_empty) == 0);
  assert(pthread_cond_destroy(&q->not_full) == 0);
}
void queue_put(struct queue *q, int value)
{
  /* TODO: implement blocking enqueue. */
  (void)q;
  (void)value;
}
int queue_get(struct queue *q)
{
  /* TODO: implement blocking dequeue. This placeholder is not a solution. */
  (void)q;
  return -1;
}
