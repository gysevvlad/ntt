#include "./task_inl.h"

ntt_task_t *ntt_make_task(ntt_task_cb_t *cb) {
  return &ntt_make_task_impl(cb)->payload;
}

void ntt_do_task(ntt_task_t *task) { return ntt_do_task_inl(task); }

void ntt_free_task(void *task) { return ntt_free_task_inl(task); }
