#pragma once

#include "ntt/debug.h"
#include "ntt/node.h"
#include "ntt/signal.h"

#include <urcu/wfcqueue.h>

EXTERN_START

struct ntt_mpsc_queue {
  struct cds_wfcq_head head;
  struct cds_wfcq_tail tail;
  DEBUG_FIELD(int, initialized);
};

void ntt_mpsc_queue_init(struct ntt_mpsc_queue *queue);

int ntt_mpsc_queue_push(struct ntt_mpsc_queue *queue, struct ntt_node *node);

void ntt_queue_signal_push(struct ntt_mpsc_queue *queue, struct ntt_node *node,
                           struct ntt_signal *signal);

struct ntt_node *ntt_mpsc_queue_front(struct ntt_mpsc_queue *queue);

struct ntt_node *ntt_mpsc_queue_pop(struct ntt_mpsc_queue *queue, int *last);

void ntt_queue_signal_front(struct ntt_mpsc_queue *queue, struct ntt_node *node,
                            struct ntt_signal *signal);

EXTERN_STOP
