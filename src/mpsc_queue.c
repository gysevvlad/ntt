#include "ntt/mpsc_queue.h"

#include "ntt/debug.h"
#include "ntt/defs.h"
#include "ntt/node.h"
#include "ntt/signal.h"

#include <urcu/wfcqueue.h>

void ntt_mpsc_queue_init(struct ntt_mpsc_queue *queue) {
  cds_wfcq_init(&queue->head, &queue->tail);
  DEBUG_ACTION({ queue->initialized = 1; });
}

int ntt_mpsc_queue_push(struct ntt_mpsc_queue *queue, struct ntt_node *node) {
  DEBUG_ASSERT(queue->initialized == 1);

  return cds_wfcq_enqueue(&queue->head, &queue->tail, &node->node) == false ? 1
                                                                            : 0;
}

void ntt_queue_signal_push(struct ntt_mpsc_queue *queue, struct ntt_node *node,
                           struct ntt_signal *signal) {
  if (ntt_mpsc_queue_push(queue, node) == false) {
    ntt_signal_notify(signal);
  }
}

struct ntt_node *ntt_mpsc_queue_front(struct ntt_mpsc_queue *queue) {
  struct cds_wfcq_node *node = NULL;
  do {
    node = __cds_wfcq_first_nonblocking(&queue->head, &queue->tail);
  } while (node == CDS_WFCQ_WOULDBLOCK);

  DEBUG_ASSERT(node != CDS_WFCQ_WOULDBLOCK);

  return ntt_container_of(node, struct ntt_node, node);
}

struct ntt_node *ntt_mpsc_queue_pop(struct ntt_mpsc_queue *queue, int *last) {
  DEBUG_ASSERT(queue->initialized == true);

  struct cds_wfcq_node *node;
  do {
    node = __cds_wfcq_dequeue_with_state_nonblocking(&queue->head, &queue->tail,
                                                     last);
  } while (node == NULL || node == CDS_WFCQ_WOULDBLOCK);

  return ntt_container_of(node, struct ntt_node, node);
}
