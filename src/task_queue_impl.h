#pragma once

#include "ntt/impl/list.h"
#include "ntt/impl/task.h"

#include <assert.h>
#include <threads.h>

typedef struct ntt_task_queue {
    ntt_list_t queues[2];
    mtx_t mtx;
    cnd_t cnd;
} ntt_task_queue_t;

static inline void ntt_task_queue_init_impl(ntt_task_queue_t* self)
{
    ntt_list_init(&self->queues[0]);
    ntt_list_init(&self->queues[1]);
    int rc = mtx_init(&self->mtx, mtx_plain);
    assert(rc == thrd_success);
    cnd_init(&self->cnd);
}

static inline void ntt_task_queue_push_back_hp_impl(ntt_task_queue_t* self,
    ntt_task_node_t* task,
    int* first)
{
    mtx_lock(&self->mtx);
    ntt_list_push_back(&self->queues[0], &task->node, first);
    mtx_unlock(&self->mtx);
    cnd_signal(&self->cnd);
}

static inline void ntt_task_queue_push_back_impl(ntt_task_queue_t* self,
    ntt_task_node_t* task,
    int* first)
{
    mtx_lock(&self->mtx);
    ntt_list_push_back(&self->queues[1], &task->node, first);
    mtx_unlock(&self->mtx);
    cnd_signal(&self->cnd);
}

static inline ntt_task_node_t*
ntt_task_queue_pop_front_blocking_impl(ntt_task_queue_t* self, int* last)
{
    mtx_lock(&self->mtx);
    ntt_node_t* node = ntt_list_pop_front(&self->queues[0], last);
    if (node == NULL) {
        node = ntt_list_pop_front(&self->queues[1], last);
    }
    while (node == NULL) {
        cnd_wait(&self->cnd, &self->mtx);
        node = ntt_list_pop_front(&self->queues[1], last);
        if (node == NULL) {
            node = ntt_list_pop_front(&self->queues[1], last);
        }
    }
    mtx_unlock(&self->mtx);
    return ntt_container_of(node, ntt_task_node_t, node);
}

static inline void ntt_task_queue_destroy_impl(ntt_task_queue_t* self)
{
    mtx_destroy(&self->mtx);
    cnd_destroy(&self->cnd);
}
