#pragma once

#include "ntt/defs.h"
#include "ntt/impl/node.h"
#include "ntt/impl/util.h"
#include "ntt/task.h"

#include <assert.h>
#include <stdlib.h>

EXTERN_START

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
    task_node->task_cb = task_cb;
    task_node->free_cb = free_cb;
    return &task_node->payload;
}

static inline ntt_task_node_t* ntt_make_task_impl(ntt_task_cb_t* task_cb,
    ntt_free_cb_t* free_cb)
{
    ntt_task_node_t* task_node = (ntt_task_node_t*)malloc(sizeof(ntt_task_node_t));
    assert(ntt_is_aligned(&task_node->payload, NTT_TASK_PAYLOAD_ALIGN) && "ntt exception: wrong assumption about task payload alignment");
    task_node->task_cb = task_cb;
    task_node->free_cb = free_cb == NULL ? free : free_cb;
    return task_node;
}

static inline int ntt_task_is_null(ntt_task_t* task)
{
    ntt_task_node_t* task_node = ntt_container_of(task, ntt_task_node_t, payload);
    return task_node->task_cb == NULL;
}

static inline void ntt_do_task_inl(ntt_task_t* task)
{
    ntt_task_node_t* task_node = ntt_container_of(task, ntt_task_node_t, payload);
    task_node->task_cb(&task_node->payload);
}

static inline void ntt_free_task_inl(ntt_task_t* task)
{
    ntt_task_node_t* task_node = ntt_container_of(task, ntt_task_node_t, payload);
    task_node->free_cb(task_node);
}

EXTERN_STOP
