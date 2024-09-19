#include "ntt/thread_pool.h"
#include "ntt/malloc.h"
#include "thread_pool_impl.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>
#include <threads.h>

void ntt_thread_pool_release_internal_ref(struct ntt_thread_pool *thread_pool) {
  size_t prev = atomic_fetch_sub(&thread_pool->internal_refs, 1);
  assert(prev > 0);

  if (prev == 1) {
    void (*complete_cb)(void *complete_arg) = thread_pool->config.complete_cb;
    void *complete_arg = thread_pool->config.complete_arg;
    ntt_free(thread_pool->threads);
    ntt_free(thread_pool);
    complete_cb(complete_arg);
  }
}

void ntt_thread_pool_configure(struct ntt_thread_pool *thread_pool) {
  thrd_t thrd = thrd_current();
  for (unsigned i = 0; i < thread_pool->config.width; ++i) {
    if (thrd == thread_pool->threads[i]) {
      if (thread_pool->config.configure_cb) {
        thread_pool->config.configure_cb(thread_pool->config.configure_arg, i);
      }
      return;
    }
  }
  assert(false);
}

int ntt_thread_pool_svc(void *arg) {
  struct ntt_thread_pool *thread_pool = arg;

  mtx_lock(&thread_pool->mtx);
  while (!thread_pool->start) {
    cnd_wait(&thread_pool->cnd, &thread_pool->mtx);
  }
  mtx_unlock(&thread_pool->mtx);

  ntt_thread_pool_configure(thread_pool);
  thread_pool->config.svc_cb(thread_pool->config.svc_arg);

  ntt_thread_pool_release_internal_ref(thread_pool);
  return 0;
}

struct ntt_thread_pool *
ntt_thread_pool_create_from_config(const struct ntt_thread_pool_config *config) {

  struct ntt_thread_pool *thread_pool =
      ntt_malloc(sizeof(struct ntt_thread_pool));

  memcpy(&thread_pool->config, config, sizeof(struct ntt_thread_pool_config));

  thread_pool->external_refs = 1;
  thread_pool->internal_refs = thread_pool->config.width + 1;

  thread_pool->threads = ntt_calloc(thread_pool->config.width, sizeof(thrd_t));

  mtx_init(&thread_pool->mtx, mtx_plain);
  cnd_init(&thread_pool->cnd);

  mtx_lock(&thread_pool->mtx);
  for (unsigned i = 0; i < thread_pool->config.width; ++i) {
    thrd_create(&thread_pool->threads[i], ntt_thread_pool_svc, thread_pool);
  }
  thread_pool->start = true;
  cnd_broadcast(&thread_pool->cnd);
  mtx_unlock(&thread_pool->mtx);

  for (unsigned i = 0; i < thread_pool->config.width; ++i) {
    thrd_detach(thread_pool->threads[i]);
  }

  return thread_pool;
}

void ntt_thread_pool_release(struct ntt_thread_pool *thread_pool) {
  size_t prev = atomic_fetch_sub(&thread_pool->external_refs, 1);
  assert(prev > 0 && "internal error: thread pool already destroyed");

  if (prev == 1) {
    ntt_thread_pool_release_internal_ref(thread_pool);
  }
}

void ntt_thread_pool_acquire(struct ntt_thread_pool *thread_pool) {
  size_t prev = atomic_fetch_add(&thread_pool->external_refs, 1);
  assert(prev > 0 && "internal error: thread pool already destroyed");
}