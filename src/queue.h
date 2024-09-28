#pragma once

#include "./node.h"

#include <stddef.h>

typedef struct ntt_queue {
  ntt_node_t node;
} ntt_queue_t;

static inline void ntt_queue_init(ntt_queue_t *queue) {
  queue->node.next = &queue->node;
  queue->node.prev = &queue->node;
}

static inline int ntt_queue_empty(ntt_queue_t *queue) {
  return queue->node.next == &queue->node;
}

static inline void ntt_queue_push_back(ntt_queue_t *queue, ntt_node_t *node,
                                       int *first) {
  *first = ntt_queue_empty(queue);

  ntt_node_t *prev = &queue->node;
  ntt_node_t *next = queue->node.next;

  prev->next = node;
  next->prev = node;
  node->next = next;
  node->prev = prev;
}

static inline ntt_node_t *ntt_queue_pop_front(ntt_queue_t *queue, int *last) {
  ntt_node_t *node = queue->node.prev;
  ntt_node_t *prev = node->prev;
  ntt_node_t *next = node->next;

  prev->next = next;
  next->prev = prev;

  *last = ntt_queue_empty(queue);

  return node != &queue->node ? node : NULL;
}
