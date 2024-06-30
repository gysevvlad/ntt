#pragma once

#include "ntt/defs.h"

EXTERN_START

struct ntt_thread_pool_config {
  unsigned width;
  void *configure_arg;
  void (*configure_cb)(void *configure_arg, unsigned i);
  void *svc_arg;
  void (*svc_cb)(void *svc_arg);
  void *complete_arg;
  void (*complete_cb)(void *complete_arg);
};

struct ntt_thread_pool;

struct ntt_thread_pool *
ntt_thread_pool_create_from_config(const struct ntt_thread_pool_config *config);

void ntt_thread_pool_release(struct ntt_thread_pool *thread_pool);

EXTERN_STOP
