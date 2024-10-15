#include "ntt/queue.h"

#include "ntt/defs.h"
#include "ntt/impl/list.h"
#include "ntt/impl/task.h"
#include "ntt/pool.h"
#include "task_list.h"

#include <pthread.h>
#include <stdatomic.h>
#include <threads.h>

struct ntt_queue {
  atomic_size_t external_refs;
  ntt_task_list_t tasks;
  ntt_pool_t *pool;
  pthread_spinlock_t lock;
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
    ntt_task_t *task = ntt_task_list_front(&self->tasks);
    ntt_do_task_inl(task);
    if (t_next_queue != NULL) {
      // queue ctx switch
      pthread_spin_lock(&self->lock);
      ntt_task_list_pop(&self->tasks, &last);
      pthread_spin_unlock(&self->lock);
      if (!last) {
        // push current queue to pool
        ntt_pool_post_task(self->pool, &self->svc_task.payload);
      }
      self = t_next_queue;
      t_curr_queue = self;
      t_next_queue = NULL;
      last = 0;
    } else {
      pthread_spin_lock(&self->lock);
      ntt_task_list_pop(&self->tasks, &last);
      pthread_spin_unlock(&self->lock);
    }
    ntt_free_task_inl(task);
  } while (!last);
}

void free_svc() {}

ntt_queue_t *ntt_queue_create(ntt_pool_t *pool) {
  ntt_queue_t *self = malloc(sizeof(ntt_queue_t));
  self->external_refs = 1;
  ntt_task_list_init(&self->tasks);
  self->pool = pool;
  ntt_pool_acquire(self->pool);
  pthread_spin_init(&self->lock, PTHREAD_PROCESS_PRIVATE);
  self->svc_task.task_cb = svc;
  self->svc_task.free_cb = free_svc;
  return self;
}

ntt_task_t *ntt_queue_alloc_task(ntt_queue_t *self, ntt_task_cb_t *task_cb) {
  return ntt_pool_alloc_task(self->pool, task_cb);
}

void ntt_queue_push(ntt_queue_t *self, ntt_task_t *task) {
  int first;
  pthread_spin_lock(&self->lock);
  ntt_task_list_push(&self->tasks, task, &first);
  pthread_spin_unlock(&self->lock);
  if (first) {
    if (t_curr_queue != NULL && self != t_curr_queue && t_next_queue == NULL) {
      t_next_queue = self;
    } else {
      ntt_pool_post_task(self->pool, &self->svc_task.payload);
    }
  }
}

void ntt_queue_acquire(ntt_queue_t *queue) {
  size_t prev = atomic_fetch_add(&queue->external_refs, 1);
  assert(prev > 0 && "ntt queue already deleted");
}

void ntt_queue_destroy(ntt_queue_t *self) {
  pthread_spin_destroy(&self->lock);
  ntt_pool_release(self->pool);
}

void ntt_queue_release(ntt_queue_t *self) {
  size_t prev = atomic_fetch_sub(&self->external_refs, 1);
  assert(prev > 0 && "ntt queue already deleted");

  if (prev == 1) {
    ntt_queue_destroy(self);
    free(self);
  }
}
