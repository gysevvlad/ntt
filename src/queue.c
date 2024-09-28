#include "ntt/queue.h"

#include "./list.h"
#include "./task_inl.h"

#include "ntt/defs.h"
#include "ntt/pool.h"

#include <stdatomic.h>
#include <threads.h>

struct ntt_queue {
  atomic_size_t external_refs;
  ntt_list_t list;
  ntt_pool_t *pool;
  mtx_t mtx;
  ntt_task_node_t svc_task;
};

thread_local ntt_queue_t *t_curr_queue = NULL;
thread_local ntt_queue_t *t_next_queue = NULL;

static void svc(void *payload) {
  ntt_task_node_t *svc_task =
      ntt_container_of(payload, ntt_task_node_t, payload);
  ntt_queue_t *self = ntt_container_of(svc_task, ntt_queue_t, svc_task);
  int last;
  t_curr_queue = self;
  t_next_queue = NULL;
  do {
    ntt_node_t *node = self->list.node.prev;
    ntt_task_node_t *task_node = ntt_container_of(node, ntt_task_node_t, node);
    task_node->cb(task_node->payload);
    if (t_next_queue != NULL) {
      // ctx switch
      mtx_lock(&self->mtx);
      ntt_list_pop_front(&self->list, &last);
      mtx_unlock(&self->mtx);
      if (!last) {
        // push current queue to pool
        ntt_pool_push(self->pool, &self->svc_task.payload);
      }
      self = t_next_queue;
      t_curr_queue = self;
      t_next_queue = NULL;
      last = 0;
    } else {
      mtx_lock(&self->mtx);
      ntt_list_pop_front(&self->list, &last);
      mtx_unlock(&self->mtx);
    }
  } while (!last);
}

ntt_queue_t *ntt_queue_create(ntt_pool_t *pool) {
  ntt_queue_t *self = malloc(sizeof(ntt_queue_t));
  self->external_refs = 1;
  ntt_list_init(&self->list);
  self->pool = pool;
  ntt_pool_acquire(self->pool);
  mtx_init(&self->mtx, mtx_plain);
  self->svc_task.cb = svc;
  return self;
}

void ntt_queue_push(ntt_queue_t *self, ntt_task_t *task) {
  ntt_task_node_t *task_node = ntt_container_of(task, ntt_task_node_t, payload);
  int first;
  mtx_lock(&self->mtx);
  ntt_list_push_back(&self->list, &task_node->node, &first);
  mtx_unlock(&self->mtx);
  if (first) {
    if (t_curr_queue != NULL && self != t_curr_queue && t_next_queue == NULL) {
      t_next_queue = self;
    } else {
      ntt_pool_push(self->pool, &self->svc_task.payload);
    }
  }
}

void ntt_queue_acquire(ntt_queue_t *queue) {
  // TODO: queue memory management
}

void ntt_queue_release(ntt_queue_t *queue) {
  // TODO: queue memory management
}
