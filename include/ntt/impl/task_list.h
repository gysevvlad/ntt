#pragma once

#include "ntt/impl/list.h"
#include "ntt/impl/task.h"
#include "ntt/task.h"

#include <threads.h>

typedef struct ntt_task_list {
  ntt_list_t list;
} ntt_task_list_t;

static inline void ntt_task_list_init(ntt_task_list_t *self) {
  ntt_list_init(&self->list);
}

static inline void ntt_task_list_push(ntt_task_list_t *self, ntt_task_t *task,
                                      int *first) {
  ntt_task_node_t *task_node = ntt_container_of(task, ntt_task_node_t, payload);
  ntt_list_push_back(&self->list, &task_node->node, first);
}

static inline ntt_task_t *ntt_task_list_front(ntt_task_list_t *self) {
  ntt_node_t *node = ntt_list_front(&self->list);
  if (node == NULL) {
    return NULL;
  }
  ntt_task_node_t *task_node = ntt_container_of(node, ntt_task_node_t, node);
  return &task_node->payload;
}

static inline ntt_task_t *ntt_task_list_pop(ntt_task_list_t *self, int *last) {
  ntt_node_t *node = ntt_list_pop_front(&self->list, last);
  if (node == NULL) {
    return NULL;
  }
  ntt_task_node_t *task_node = ntt_container_of(node, ntt_task_node_t, node);
  return &task_node->payload;
}
