#pragma once

#include "ntt/defs.h"
#include "ntt/impl/ntt_task_cache.h"
#include "ntt/impl/task.h"
#include "task_list.h"
#include <pthread.h>

typedef struct ntt_task_slot {
  ntt_task_cache_t *cache;
  ntt_task_node_t task_node;
} ntt_task_slot_t;

struct ntt_task_cache {
  ntt_task_list_t free_list;
  pthread_spinlock_t lock;
  ntt_task_slot_t slots[];
};

#define NTT_TASK_CACHE_SIZE 1024 * 128

void ntt_task_cache_free(void *arg) {
  ntt_task_node_t *task_node = arg;
  ntt_task_slot_t *slot =
      ntt_container_of(task_node, ntt_task_slot_t, task_node);
  int first;
  pthread_spin_lock(&slot->cache->lock);
  ntt_task_list_push(&slot->cache->free_list, &slot->task_node.payload, &first);
  pthread_spin_unlock(&slot->cache->lock);
}

ntt_task_cache_t *ntt_task_cache_create() {
  ntt_task_cache_t *self = malloc(
      sizeof(ntt_task_cache_t) + sizeof(ntt_task_slot_t) * NTT_TASK_CACHE_SIZE);
  ntt_task_list_init(&self->free_list);
  pthread_spin_init(&self->lock, PTHREAD_PROCESS_PRIVATE);
  int i;
  for (i = 0; i < NTT_TASK_CACHE_SIZE; ++i) {
    self->slots[i].cache = self;
    self->slots[i].task_node.free_cb = ntt_task_cache_free;
    int first;
    ntt_task_list_push(&self->free_list, &self->slots[i].task_node.payload,
                       &first);
  }
  return self;
}

ntt_task_t *ntt_task_cache_alloc_task(ntt_task_cache_t *self,
                                      ntt_task_cb_t *task_cb) {
  int last;
  pthread_spin_lock(&self->lock);
  ntt_task_t *task = ntt_task_list_pop(&self->free_list, &last);
  pthread_spin_unlock(&self->lock);
  if (task == NULL) {
    return &ntt_make_task_impl(task_cb, NULL)->payload;
  }
  ntt_task_node_t *task_node = ntt_container_of(task, ntt_task_node_t, payload);
  task_node->task_cb = task_cb;
  return task;
}

void ntt_task_cache_destroy(ntt_task_cache_t *self) {}
