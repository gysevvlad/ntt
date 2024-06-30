#include "ntt/thread_pool.h"
#include "ntt/malloc.h"
#include <stdbool.h>
#include <threads.h>

void ntt_thread_pool_init(struct ntt_thread_pool *thread_pool, size_t width,
                          thrd_start_t start, void *arg,
                          void (*stop_callback)(void *),
                          void *stop_callback_arg) {
  thread_pool->width = width;
  thread_pool->threads = ntt_calloc(width, sizeof(thrd_t));
  thread_pool->stop_callback = stop_callback;
  thread_pool->stop_callback_arg = stop_callback_arg;
  for (size_t i = 0; i < width; ++i) {
    thrd_create(&thread_pool->threads[i], start, arg);
  }
}

struct stop_context {
  void *stop_callback_arg;
  void (*stop_callback)(void *stop_callback_arg);
  thrd_t join_thread;
};

int ntt_join_stop(void *arg) {
  struct stop_context *ctx = arg;
  int res;
  thrd_join(ctx->join_thread, &res);
  ctx->stop_callback(ctx->stop_callback_arg);
  ntt_free(ctx);
  return 0;
}

void ntt_thread_pool_destroy(struct ntt_thread_pool *thread_pool) {
  bool self_destroy = false;
  for (size_t i = 0; i < thread_pool->width; ++i) {
    if (thread_pool->threads[i] != thrd_current()) {
      thrd_join(thread_pool->threads[i], NULL);
    } else {
      self_destroy = true;
      struct stop_context *ctx = ntt_malloc(sizeof(struct stop_context));
      ctx->stop_callback = thread_pool->stop_callback;
      ctx->stop_callback_arg = thread_pool->stop_callback_arg;
      ctx->join_thread = thread_pool->threads[i];
      thrd_t stop_thread;
      thrd_create(&stop_thread, ntt_join_stop, ctx);
      thrd_detach(stop_thread);
    }
  }
  ntt_free(thread_pool->threads);
  if (!self_destroy) {
    thread_pool->stop_callback(thread_pool->stop_callback_arg);
  }
}
