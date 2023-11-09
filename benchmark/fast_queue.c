#include "../source/common/c_queue.h"

#include <stdio.h>

typedef struct node_t {
  float value;
} node_t;

int comp(const void *a, const void *b) {
  return ((node_t *)a)->value - ((node_t *)b)->value;
}

int main(int argc, char const *argv[]) {
  queue_t queue;

  C_QueueNew(&queue, comp, 1000);

  C_QueueEnqueue(&queue, &(node_t){150});
  C_QueueEnqueue(&queue, &(node_t){500});
  C_QueueEnqueue(&queue, &(node_t){100});
  C_QueueEnqueue(&queue, &(node_t){10});

  for (unsigned i = 0; i < 4; i++) {
    printf("%.0f\n", ((node_t *)C_QueueDequeue(&queue))->value);
  }
  
  return 0;
}
