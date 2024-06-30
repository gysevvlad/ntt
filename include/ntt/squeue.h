#pragma once

#include "ntt/defs.h"
#include "ntt/mpsc_queue.h"
#include "ntt/task_queue.h"

EXTERN_START

struct ntt_squeue {
  struct ntt_mpsc_queue mpsc_queue;
  struct ntt_task_queue *task_queue;
  struct ntt_task_node task_node;
};

struct ntt_sq_task {
  void (*svc)(void *arg);
  void (*del)(void *arg);
  void *arg;
  struct ntt_node node;
};

NTT_EXPORT void ntt_squeue_init(struct ntt_squeue *squeue,
                                struct ntt_task_queue *task_queue);

NTT_EXPORT void ntt_squeue_dispatch(struct ntt_squeue *queue,
                                    struct ntt_sq_task *node);

NTT_EXPORT void ntt_squeue_destroy(struct ntt_squeue *queue);

EXTERN_STOP
