#pragma once

#include "ntt/defs.h"
#include "ntt/event_queue.h"

EXTERN_START

struct ntt_work_loop_config {
  unsigned width;
  void *configure_arg;
  void (*configure_cb)(void *configure_arg, unsigned i);
  void *complete_arg;
  void (*complete_cb)(void *complete_arg);
};

struct ntt_work_loop;

NTT_EXPORT struct ntt_work_loop *
ntt_work_loop_create_from_config(const struct ntt_work_loop_config *config);

NTT_EXPORT void ntt_work_loop_release(struct ntt_work_loop *work_loop);

NTT_EXPORT struct ntt_work_loop *
ntt_work_loop_acquire(struct ntt_work_loop *work_loop);

NTT_EXPORT void ntt_work_loop_dispatch(struct ntt_work_loop *work_loop,
                                       struct ntt_task_node *task_node);

EXTERN_STOP
