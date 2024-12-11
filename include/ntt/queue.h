#pragma once

#include "ntt/defs.h"
#include "ntt/export.h"
#include "ntt/pool.h"

EXTERN_START

typedef struct ntt_queue ntt_queue_t;

NTT_EXPORT ntt_queue_t* ntt_queue_create(ntt_pool_t* pool);
NTT_EXPORT void ntt_queue_acquire(ntt_queue_t* self);
NTT_EXPORT void ntt_queue_release(ntt_queue_t* self);
NTT_EXPORT ntt_task_t* ntt_queue_alloc_task(ntt_queue_t* self,
    ntt_task_cb_t* task_cb);
NTT_EXPORT void ntt_queue_push(ntt_queue_t* self, ntt_task_t* task);

EXTERN_STOP
