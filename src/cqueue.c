#include "ntt/cqueue.h"

void ntt_cqueue_init(struct ntt_cqueue *queue) {
  int rc;

  ntt_mpsc_queue_init(&queue->queue);

  rc = pthread_spin_init(&queue->lock, PTHREAD_PROCESS_PRIVATE);
  DEBUG_ASSERT(rc == 0);
}

void ntt_cqueue_push(struct ntt_cqueue *queue, struct ntt_forward_node *node) {
  ntt_mpsc_queue_push(&queue->queue, node);
}

struct ntt_node *ntt_cqueue_pop(struct ntt_cqueue *queue) {
  int rc;

  rc = pthread_spin_lock(&queue->lock);
  DEBUG_ASSERT(rc == 0);

  struct ntt_node *node = ntt_mpsc_queue_pop(&queue->queue);

  rc = pthread_spin_unlock(&queue->lock);
  DEBUG_ASSERT(rc == 0);

  return node;
}

void ntt_cqueue_destroy(struct ntt_cqueue *queue) {}
