#include "ntt/work_loop.h"
#include "ntt/event_queue.h"
#include "ntt/malloc.h"
#include "ntt/thread_pool.h"
#include "work_loop_impl.h"
#include <assert.h>
#include <stdatomic.h>

void ntt_work_loop_release_internal_ref(struct ntt_work_loop *work_loop) {
  size_t prev = atomic_fetch_sub(&work_loop->internal_refs, 1);
  assert(prev > 0 && "internal error: work loop already destroyed");

  if (prev == 1) {
    ntt_free(work_loop);
  }
}

void ntt_work_loop_release_external_ref(struct ntt_work_loop *work_loop) {
  size_t prev = atomic_fetch_sub(&work_loop->external_refs, 1);
  assert(prev > 0 && "internal error: work loop already destroyed");

  if (prev == 1) {
    ntt_event_queue_stop(&work_loop->event_queue);
    ntt_thread_pool_release(work_loop->thread_pool);
    ntt_work_loop_release_internal_ref(work_loop);
  }
}

void ntt_work_loop_svc(void *arg) {
  struct ntt_work_loop *work_loop = arg;
  struct ntt_task_node *node = NULL;
  while ((node = ntt_event_queue_pop(&work_loop->event_queue))) {
    node->svc(node->svc_arg);
  }
  ntt_work_loop_release_internal_ref(work_loop);
}

void ntt_work_loop_release(struct ntt_work_loop *work_loop) {
  ntt_work_loop_release_external_ref(work_loop);
}

struct ntt_work_loop *ntt_work_loop_acquire(struct ntt_work_loop *work_loop) {
  size_t prev = atomic_fetch_add(&work_loop->external_refs, 1);
  assert(prev > 0 && "internal error: work loop already destroyed");
  return work_loop;
}

struct ntt_work_loop *
ntt_work_loop_create_from_config(const struct ntt_work_loop_config *config) {

  struct ntt_work_loop *work_loop = ntt_malloc(sizeof(struct ntt_work_loop));

  work_loop->external_refs = 1;
  work_loop->internal_refs = config->width + 1;

  ntt_event_queue_init(&work_loop->event_queue);

  struct ntt_thread_pool_config thread_pool_config = {
      .width = config->width,
      .configure_arg = config->configure_arg,
      .configure_cb = config->configure_cb,
      .svc_arg = work_loop,
      .svc_cb = ntt_work_loop_svc,
      .complete_arg = config->complete_arg,
      .complete_cb = config->complete_cb,
  };

  work_loop->thread_pool =
      ntt_thread_pool_create_from_config(&thread_pool_config);

  return work_loop;
}

void ntt_work_loop_dispatch(struct ntt_work_loop *work_loop,
                            struct ntt_task_node *task_node) {
  ntt_event_queue_push(&work_loop->event_queue, task_node);
}
