#pragma once

#include "./task_queue_impl.h"

#include <stdatomic.h>
#include <threads.h>

struct ntt_pool {
  atomic_size_t external_refs;
  atomic_size_t internal_refs;
  ntt_task_queue_t task_queue;
  unsigned short width;
};
