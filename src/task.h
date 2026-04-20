#pragma once

#include "./malloc.h"
#include "./node.h"
#include "ntt/defs.h"
#include "ntt/task.h"

EXTERN_START

#include <assert.h>
#include <stdlib.h>

typedef struct ntt_task_node {
    ntt_node_t node;
    ntt_task_cb_t* task_cb;
    ntt_free_cb_t* free_cb;
    // TODO: use enum with ptr and payload
    char payload[32];
} ntt_task_node_t;

static inline ntt_task_t* ntt_task_init(ntt_task_node_t* task_node,
    ntt_task_cb_t* task_cb,
    ntt_free_cb_t* free_cb)
{
    ntt_node_init(&task_node->node);
    task_node->task_cb = task_cb;
    task_node->free_cb = free_cb;
    return &task_node->payload;
}

static inline int ntt_is_aligned(void* ptr, size_t align)
{
    return !((uintptr_t)(ptr) % align);
}

/**
 * @brief Check that task unlinked.
 *
 * @return Returns 1 if task unlinked, otherwise 0.
 */
static inline int ntt_task_unlinked(ntt_task_t* self)
{
    ntt_task_node_t* task_node = ntt_container_of(self, ntt_task_node_t, payload);
    return ntt_node_unlinked(&task_node->node);
}

static inline ntt_task_node_t* ntt_task_create_inl(
    ntt_task_cb_t* task_cb,
    ntt_free_cb_t* free_cb)
{
    ntt_task_node_t* self = (ntt_task_node_t*)ntt_malloc(sizeof(ntt_task_node_t));
    assert(ntt_is_aligned(&self->payload, NTT_TASK_PAYLOAD_ALIGN) && "ntt exception: wrong assumption about task payload alignment");
    ntt_node_init(&self->node);
    self->task_cb = task_cb;
    self->free_cb = free_cb == NULL ? ntt_free : free_cb;
    return self;
}

static inline int ntt_task_is_null(ntt_task_t* task)
{
    ntt_task_node_t* task_node = ntt_container_of(task, ntt_task_node_t, payload);
    return task_node->task_cb == NULL;
}

static inline void ntt_task_svc_inl(ntt_task_t* task)
{
    ntt_task_node_t* task_node = ntt_container_of(task, ntt_task_node_t, payload);
    task_node->task_cb(&task_node->payload);
}

static inline void ntt_task_destroy_inl(ntt_task_t* task)
{
    ntt_task_node_t* task_node = ntt_container_of(task, ntt_task_node_t, payload);
    task_node->free_cb(task_node);
}

EXTERN_STOP
