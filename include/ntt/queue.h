#pragma once

#include "ntt/defs.h"
#include "ntt/export.h"
#include "ntt/pool.h"

EXTERN_START

typedef struct ntt_queue ntt_queue_t;

NTT_EXPORT ntt_queue_t *ntt_queue_create(ntt_pool_t *pool);
NTT_EXPORT void ntt_queue_acquire(ntt_queue_t *pool);
NTT_EXPORT void ntt_queue_release(ntt_queue_t *pool);

NTT_EXPORT void ntt_queue_push(ntt_queue_t *queue, ntt_task_t *task);

EXTERN_STOP
