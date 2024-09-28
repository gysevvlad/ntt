#pragma once

#include "./queue.h"
#include "./task_inl.h"

#include <threads.h>

typedef struct ntt_task_queue {
  ntt_queue_t queue;
  mtx_t mtx;
  cnd_t cnd;
} ntt_task_queue_t;

static inline void ntt_task_queue_init_impl(ntt_task_queue_t *self) {
  ntt_queue_init(&self->queue);
  int rc = mtx_init(&self->mtx, mtx_plain);
  assert(rc == thrd_success);
  cnd_init(&self->cnd);
}

static inline void ntt_task_queue_push_back_impl(ntt_task_queue_t *self,
                                                 ntt_task_node_t *task,
                                                 int *first) {
  mtx_lock(&self->mtx);
  ntt_queue_push_back(&self->queue, &task->node, first);
  cnd_signal(&self->cnd);
  mtx_unlock(&self->mtx);
}

static inline ntt_task_node_t *
ntt_task_queue_pop_front_blocking_impl(ntt_task_queue_t *self, int *last) {
  mtx_lock(&self->mtx);
  ntt_node_t *node = ntt_queue_pop_front(&self->queue, last);
  while (node == NULL) {
    cnd_wait(&self->cnd, &self->mtx);
    node = ntt_queue_pop_front(&self->queue, last);
  }
  mtx_unlock(&self->mtx);
  return ntt_container_of(node, ntt_task_node_t, node);
}

static inline void ntt_task_queue_destroy_impl(ntt_task_queue_t *self) {
  mtx_destroy(&self->mtx);
  cnd_destroy(&self->cnd);
}
