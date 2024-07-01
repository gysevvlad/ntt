#pragma once

#include "ntt/defs.h"
#include "ntt/export.h"
#include "ntt/node.h"
#include "ntt/work_loop.h"

EXTERN_START

struct ntt_sq_task {
  void (*svc)(void *arg);
  void (*del)(void *arg);
  void *arg;
  struct ntt_node node;
};

NTT_EXPORT struct ntt_task_queue *
ntt_task_queue_create(struct ntt_work_loop *work_loop);

NTT_EXPORT void ntt_task_queue_release(struct ntt_task_queue *task_queue);

NTT_EXPORT void ntt_task_queue_acquire(struct ntt_task_queue *task_queue);

NTT_EXPORT void ntt_task_queue_dispatch(struct ntt_task_queue *task_queue,
                                        struct ntt_sq_task *node);

EXTERN_STOP
