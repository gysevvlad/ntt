#pragma once

#include "ntt/defs.h"
#include "ntt/event_queue.h"
#include "ntt/thread_pool.h"

#include <stdatomic.h>

EXTERN_START

struct ntt_work_loop {
  atomic_size_t external_refs;
  atomic_size_t internal_refs;
  struct ntt_event_queue event_queue;
  struct ntt_thread_pool *thread_pool;
};

EXTERN_STOP
