#include "./pool_inl.h"
#include "./task_queue_impl.h"

#include "ntt/pool.h"

#include <stdatomic.h>
#include <threads.h>

void ntt_pool_destroy(ntt_pool_t *self) {
  assert(self->external_refs == 0);
  assert(self->internal_refs == 0);
  ntt_task_queue_destroy_impl(&self->task_queue);
}

int ntt_pool_svc(void *arg) {
  ntt_pool_t *pool = arg;

  int last;

  for (;;) {
    ntt_task_node_t *task_node =
        ntt_task_queue_pop_front_blocking_impl(&pool->task_queue, &last);
    if (task_node->cb == NULL) {
      break;
    }
    task_node->cb(task_node->payload);
    free(task_node);
  }

  size_t prev = atomic_fetch_sub(&pool->internal_refs, 1);
  if (prev == 1) {
    assert(ntt_queue_empty(&pool->task_queue.queue));
    ntt_pool_destroy(pool);
    free(pool);
  }

  return 0;
}

ntt_pool_t *ntt_pool_create(unsigned short width) {
  ntt_pool_t *self = malloc(sizeof(ntt_pool_t));
  self->internal_refs = width;
  self->external_refs = 1;
  ntt_task_queue_init_impl(&self->task_queue);
  self->width = width;
  for (unsigned short i = 0; i < width; ++i) {
    thrd_t thrd;
    int rc = thrd_create(&thrd, ntt_pool_svc, self);
    assert(rc == thrd_success);
    rc = thrd_detach(thrd);
    assert(rc == thrd_success);
  }
  return self;
}

void ntt_pool_push(ntt_pool_t *self, ntt_task_t *task) {
  int first;
  ntt_task_node_t *task_node = ntt_container_of(task, ntt_task_node_t, payload);
  ntt_task_queue_push_back_impl(&self->task_queue, task_node, &first);
}

void ntt_pool_acquire(ntt_pool_t *self) {
  assert(self != NULL);

  size_t prev = atomic_fetch_add(&self->external_refs, 1);
  assert(prev > 0);
}

void ntt_pool_release(ntt_pool_t *self) {
  assert(self != NULL);

  size_t prev = atomic_fetch_sub(&self->external_refs, 1);
  assert(prev > 0);

  if (prev == 1) {
    for (unsigned int i = 0; i < self->width; ++i) {
      int first;
      ntt_task_queue_push_back_impl(&self->task_queue, ntt_make_task_impl(NULL),
                                    &first);
    }
  }
}
