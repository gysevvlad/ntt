#include "ntt/task_queue.h"

#include "./task_queue_impl.h"

#include <stdlib.h>

ntt_task_queue_t *ntt_task_queue_create() {
  ntt_task_queue_t *self = malloc(sizeof(ntt_task_queue_t));
  ntt_task_queue_init_impl(self);
  return self;
}

void ntt_task_queue_free(ntt_task_queue_t *self) {
  ntt_task_queue_destroy_impl(self);
  free(self);
}

void ntt_task_queue_push_back(ntt_task_queue_t *self, ntt_task_t *task,
                              int *last) {
  ntt_task_node_t *task_node = ntt_container_of(task, ntt_task_node_t, payload);
  ntt_task_queue_push_back_impl(self, task_node, last);
}

ntt_task_t *ntt_task_queue_pop_front_blocking(ntt_task_queue_t *self,
                                              int *last) {
  return &ntt_task_queue_pop_front_blocking_impl(self, last)->payload;
}
