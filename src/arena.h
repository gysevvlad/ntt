#pragma once

#include "ntt/defs.h"

#include "./task.h"

typedef struct ntt_task_cache ntt_task_cache_t;

ntt_task_cache_t* ntt_task_cache_create();

ntt_task_t* ntt_task_cache_alloc_task(ntt_task_cache_t* self, ntt_task_cb* task_cb);

void ntt_task_cache_destroy(ntt_task_cache_t* self);
