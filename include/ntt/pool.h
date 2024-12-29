#pragma once

#include "ntt/defs.h"
#include "ntt/export.h"
#include "ntt/task.h"

EXTERN_START

typedef struct ntt_pool ntt_pool_t;
typedef struct ntt_source ntt_source_t;

// TODO: provide commin ntt_callback_t
typedef void(ntt_pool_stopped_cb)(void* ctx);

NTT_EXPORT ntt_pool_t* ntt_pool_create(unsigned short width);

NTT_EXPORT ntt_pool_t* ntt_pool_with_stopped_cb(
    unsigned short width,
    ntt_pool_stopped_cb* stopped_cb,
    void* stopped_ctx);

NTT_EXPORT void ntt_pool_wakeup_all(ntt_pool_t* pool);

NTT_EXPORT void ntt_pool_post_task(ntt_pool_t* self, ntt_task_t* task);

/**
 * @brief Push task to local thread queue.
 */
NTT_EXPORT void ntt_pool_push_task(ntt_pool_t* self, ntt_task_t* task);

NTT_EXPORT void ntt_pool_send_task(ntt_pool_t* pool, unsigned short idx, ntt_task_t* task);

NTT_EXPORT ntt_task_t* ntt_pool_alloc_task(ntt_pool_t* pool, ntt_task_cb_t* task_cb);

NTT_EXPORT void ntt_pool_post_barrier_task(ntt_pool_t* self, ntt_task_t* task);

NTT_EXPORT ntt_pool_t* ntt_pool_acquire(ntt_pool_t* self);

NTT_EXPORT void ntt_pool_release(ntt_pool_t* self);

NTT_EXPORT void ntt_pool_attach_source(ntt_pool_t* self, ntt_source_t* source);

NTT_EXPORT void ntt_pool_detach_source(ntt_pool_t* self, ntt_source_t* source);

EXTERN_STOP