#pragma once

#include <stdlib.h>

typedef int queue_comp_t(const void *a, const void *b);

typedef struct queue_t {
  int (*comp)(const void *a, const void *b);
  
  size_t capacity;
  size_t size;
  void **data;
} queue_t;

void C_QueueNew(queue_t *queue, queue_comp_t comparator, size_t capacity);

void C_QueueEnqueue(queue_t *queue, void *element);

void *C_QueueDequeue(queue_t *queue);

void C_QueueDestroy(queue_t *queue);
