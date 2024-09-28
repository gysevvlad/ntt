#pragma once

#include "./node.h"

#include <stddef.h>

typedef struct ntt_list {
  ntt_node_t node;
} ntt_list_t;

static inline void ntt_list_init(ntt_list_t *queue) {
  queue->node.next = &queue->node;
  queue->node.prev = &queue->node;
}

static inline int ntt_list_empty(ntt_list_t *queue) {
  return queue->node.next == &queue->node;
}

static inline void ntt_list_push_back(ntt_list_t *queue, ntt_node_t *node,
                                      int *first) {
  *first = ntt_list_empty(queue);

  ntt_node_t *prev = &queue->node;
  ntt_node_t *next = queue->node.next;

  prev->next = node;
  next->prev = node;
  node->next = next;
  node->prev = prev;
}

static inline ntt_node_t *ntt_list_pop_front(ntt_list_t *queue, int *last) {
  ntt_node_t *node = queue->node.prev;
  ntt_node_t *prev = node->prev;
  ntt_node_t *next = node->next;

  prev->next = next;
  next->prev = prev;

  *last = ntt_list_empty(queue);

  return node != &queue->node ? node : NULL;
}
