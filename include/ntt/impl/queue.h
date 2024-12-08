#pragma once

#include "ntt/impl/task_list.h"

#include <stdatomic.h>

typedef struct ntt_pool ntt_pool_t;

struct ntt_queue {
  atomic_size_t external_refs;
  ntt_task_list_t tasks;
  ntt_pool_t *pool;
  pthread_spinlock_t lock;
  ntt_task_node_t svc_task;
};
