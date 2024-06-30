#include "ntt/thread_pool.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <threads.h>

struct ntt_thread_pool {
  atomic_size_t external_refs;
  atomic_size_t internal_refs;
  struct ntt_thread_pool_config config;
  thrd_t *threads;
  mtx_t mtx;
  cnd_t cnd;
  bool start;
};
