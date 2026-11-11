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
  pthread_mutex_lock(&q->mutex);  
  while (q->count == q->capacity) {
    pthread_cond_wait(&q->not_full, &q->mutex);
  }
  q->data[q->tail] = value;
  q->tail = (q->tail + 1) % q->capacity;
  q->count++;
  pthread_cond_signal(&q->not_empty);
  pthread_mutex_unlock(&q->mutex);
}
int queue_get(struct queue *q)
{
  /* TODO: implement blocking dequeue. This placeholder is not a solution. */
  pthread_mutex_lock(&q->mutex);
  while (q->count == 0) {
    pthread_cond_wait(&q->not_empty, &q->mutex);
  }
  int value = q->data[q->head];
  q->head = (q->head + 1) % q->capacity;
  q->count--;
  pthread_cond_signal(&q->not_full);
  pthread_mutex_unlock(&q->mutex);
  return value;
}
