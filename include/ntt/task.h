#pragma once

#include <urcu/wfcqueue.h>

typedef void(ntt_callback)(void *context);

struct ntt_task {
  struct cds_wfcq_node node;
  void *context;
  ntt_callback *callback;
};

void ntt_task_init(struct ntt_task *task, void *context,
                   ntt_callback *callback);
