#include "c_queue.h"

#include <stdio.h>
#include <string.h>

#define LEFT(x) (2 * (x) + 1)
#define RIGHT(x) (2 * (x) + 2)
#define PARENT(x) ((x) / 2)

void C_QueueHeapify(queue_t *q, size_t idx) {}

void C_QueueNew(queue_t *queue, queue_comp_t comparator, size_t capacity) {
  queue->comp = comparator;
  queue->capacity = capacity;
  queue->data = calloc(capacity, sizeof(void *));
  queue->size = 0;
}

void C_QueueEnqueue(queue_t *q, void *data) {
  // printf("enqueue %d\n", q->size);
  if (q->size >= q->capacity) {
    printf("You really stupid... This queue is full %ld >= %ld!\n", q->size,
           q->capacity);
    exit(-1);
  }
  q->data[q->size] = data;
  q->size++;

  // Sort from the bottom
  size_t idx = q->size - 1;

  while (idx > 0 && q->comp(q->data[idx], q->data[idx - 1]) * -1.0 >= 0) {
    void *tmp = q->data[idx];
    q->data[idx] = q->data[idx - 1];
    q->data[idx - 1] = tmp;
    idx -= 1;
  }
}

void *C_QueueDequeue(queue_t *q) {
  // printf("dequeue %d\n", q->size);
  if (q->size == 0) {
    printf("You stupid? This queue is empty.\n");
    exit(-1);
  }
  q->size--;
  void *data = q->data[q->size];

  return data;
}

void C_QueueDestroy(queue_t *queue) { free(queue->data); }
