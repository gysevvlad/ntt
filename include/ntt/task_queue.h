#pragma once

#include "ntt/defs.h"
#include "ntt/export.h"

#include <threads.h>

EXTERN_START

struct ntt_task_node {
  void (*svc)(void *);
  void *svc_arg;
  struct ntt_task_node *next;
};

struct ntt_task_queue {
  mtx_t m;
  cnd_t c;
  struct ntt_task_node head;
  struct ntt_task_node *tail;
  int stopped;
};

NTT_EXPORT void ntt_task_queue_init(struct ntt_task_queue *task_queue);

NTT_EXPORT void ntt_task_queue_push(struct ntt_task_queue *task_queue,
                         struct ntt_task_node *node);

NTT_EXPORT void ntt_task_queue_stop(struct ntt_task_queue *task_queue);

NTT_EXPORT struct ntt_task_node *ntt_task_queue_pop(struct ntt_task_queue *task_queue);

NTT_EXPORT void ntt_task_queue_destroy(struct ntt_task_queue *task_queue);

EXTERN_STOP
