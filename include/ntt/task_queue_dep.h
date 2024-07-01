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

struct ntt_task_queue_dep {
  mtx_t m;
  cnd_t c;
  struct ntt_task_node head;
  struct ntt_task_node *tail;
  int stopped;
};

NTT_EXPORT void
ntt_task_queue_dep_init(struct ntt_task_queue_dep *task_queue_dep);

NTT_EXPORT void
ntt_task_queue_dep_push(struct ntt_task_queue_dep *task_queue_dep,
                        struct ntt_task_node *task_node);

NTT_EXPORT void
ntt_task_queue_dep_stop(struct ntt_task_queue_dep *task_queue_dep);

NTT_EXPORT struct ntt_task_node *
ntt_task_queue_dep_pop(struct ntt_task_queue_dep *task_queue_dep);

NTT_EXPORT void
ntt_task_queue_dep_destroy(struct ntt_task_queue_dep *task_queue_dep);

EXTERN_STOP
