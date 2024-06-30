#pragma once

#include "ntt/defs.h"

#include <threads.h>

EXTERN_START

struct ntt_thread_pool {
  size_t width;
  thrd_t *threads;
  void *stop_callback_arg;
  void (*stop_callback)(void *stop_callback_arg);
};

void ntt_thread_pool_init(struct ntt_thread_pool *thread_pool, size_t width,
                          thrd_start_t start, void *arg,
                          void (*stop_callback)(void *),
                          void *stop_callback_arg);

void ntt_thread_pool_destroy(struct ntt_thread_pool *thread_pool);

EXTERN_STOP
