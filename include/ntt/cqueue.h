#pragma once

#include "ntt/defs.h"
#include "ntt/mpsc_queue.h"

#include <pthread.h>

EXTERN_START

struct ntt_cqueue {
  struct ntt_mpsc_queue queue;
  pthread_spinlock_t lock;
};

void ntt_cqueue_init(struct ntt_cqueue *queue);

void ntt_cqueue_push(struct ntt_cqueue *queue, struct ntt_node *node);

struct ntt_node *ntt_cqueue_pop(struct ntt_cqueue *queue);

void ntt_cqueue_destroy(struct ntt_cqueue *queue);

EXTERN_STOP
