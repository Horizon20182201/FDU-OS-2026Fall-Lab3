#ifndef LAB_QUEUE_H
#define LAB_QUEUE_H
#include <pthread.h>
#define QUEUE_MAX_CAPACITY 64
struct queue {
  int data[QUEUE_MAX_CAPACITY];
  int capacity;
  int head;
  int tail;
  int count;
  pthread_mutex_t mutex;
  pthread_cond_t not_empty;
  pthread_cond_t not_full;
};
/* capacity is in [1, QUEUE_MAX_CAPACITY]. */
void queue_init(struct queue *q, int capacity);
/* Call only after all worker threads have joined. */
void queue_destroy(struct queue *q);
void queue_put(struct queue *q, int value);
int queue_get(struct queue *q);
#endif
