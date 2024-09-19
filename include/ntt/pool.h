#pragma once

#include "ntt/defs.h"

EXTERN_START

typedef struct ntt_pool ntt_pool_t;

ntt_pool_t *ntt_pool_create();

void ntt_pool_acquire(ntt_pool_t *pool);

void ntt_pool_release(ntt_pool_t *pool);

void ntt_pool_post(ntt_pool_t *pool, ntt_task_t *task);

EXTERN_STOP
