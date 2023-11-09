#include "c_queue.h"

#include <stdio.h>

#define LEFT(x) (2 * (x) + 1)
#define RIGHT(x) (2 * (x) + 2)
#define PARENT(x) ((x) / 2)

void C_QueueHeapify(queue_t *q, size_t idx) {
  /* left index, right index, largest */
  void *tmp = NULL;
  size_t l_idx, r_idx, lrg_idx;

  l_idx = LEFT(idx);
  r_idx = RIGHT(idx);

  /* Left child exists, compare left child with its parent */
  if (l_idx < q->size && q->comp(q->data[l_idx], q->data[idx]) > 0) {
    lrg_idx = l_idx;
  } else {
    lrg_idx = idx;
  }

  /* Right child exists, compare right child with the largest element */
  if (r_idx < q->size && q->comp(q->data[r_idx], q->data[lrg_idx]) > 0) {
    lrg_idx = r_idx;
  }

  /* At this point largest element was determined */
  if (lrg_idx != idx) {
    /* Swap between the index at the largest element */
    tmp = q->data[lrg_idx];
    q->data[lrg_idx] = q->data[idx];
    q->data[idx] = tmp;
    /* Heapify again */
    C_QueueHeapify(q, lrg_idx);
  }
}

void C_QueueNew(queue_t *queue, queue_comp_t comparator, size_t capacity) {
  queue->comp = comparator;
  queue->capacity = capacity;
  queue->data = calloc(capacity, sizeof(void *));
  queue->size = 0;
}

void C_QueueEnqueue(queue_t *q, void *data) {
  size_t i;
  void *tmp = NULL;
  if (q->size >= q->capacity) {
    printf("Priority Queue is full. Cannot add another element. %ld >= %ld\n",
           q->size, q->capacity);
    exit(-1);
    return;
  }
  /* Adds element last */
  q->data[q->size] = (void *)data;
  i = q->size;
  q->size++;
  /* The new element is swapped with its parent as long as its
  precedence is higher */
  while (i > 0 && q->comp(q->data[i], q->data[PARENT(i)]) > 0) {
    tmp = q->data[i];
    q->data[i] = q->data[PARENT(i)];
    q->data[PARENT(i)] = tmp;
    i = PARENT(i);
  }
}

void *C_QueueDequeue(queue_t *q) {
  void *data = NULL;
  if (q->size < 1) {
    /* Priority Queue is empty */
    printf("Priority Queue underflow . Cannot remove another element.\n");
    exit(-1);
    return NULL;
  }
  data = q->data[0];
  q->data[0] = q->data[q->size - 1];
  q->size--;
  /* Restore heap property */
  C_QueueHeapify(q, 0);
  return (data);
}

void C_QueueDestroy(queue_t *queue) { free(queue->data); }
