#include "ntt/task.h"

#include "ntt/impl/task.h"

ntt_task_t* ntt_make_task(ntt_task_cb_t* task_cb, ntt_free_cb_t* free_cb)
{
    return &ntt_make_task_impl(task_cb, free_cb)->payload;
}

void ntt_do_task(ntt_task_t* task) { return ntt_do_task_inl(task); }

void ntt_free_task(void* task) { return ntt_free_task_inl(task); }
