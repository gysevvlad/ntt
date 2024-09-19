#pragma once

#include "ntt/node.h"

#include <stddef.h>

typedef struct ntt_queue {
  struct ntt_node *tail;
  struct ntt_node node;
} ntt_queue_t;

static inline void ntt_queue_init(ntt_queue_t *queue) {
  queue->node.next = &queue->node;
  queue->tail = &queue->node;
}

static inline int ntt_queue_empty(ntt_queue_t *queue) {
  return queue->node.next == &queue->node ? 1 : 0;
}

static inline void ntt_queue_push_back(ntt_queue_t *queue, ntt_node_t *node,
                                       int *first) {
  node->next = &queue->node;
  *first = ntt_queue_empty(queue);
  queue->tail->next = node;
  queue->tail = node;
}

static inline ntt_node_t *ntt_queue_pop_front(ntt_queue_t *queue, int *last) {
  ntt_node_t *node = queue->node.next;
  queue->node.next = queue->node.next->next;
  *last = ntt_queue_empty(queue);
  return node != &queue->node ? node : NULL;
}
