#pragma once

#include "ntt/defs.h"
#include "ntt/export.h"
#include "ntt/task.h"

EXTERN_START

typedef struct ntt_task_queue ntt_task_queue_t;

NTT_EXPORT ntt_task_queue_t *ntt_task_queue_create();

NTT_EXPORT void ntt_task_queue_free(ntt_task_queue_t *self);

NTT_EXPORT void ntt_task_queue_push_back(ntt_task_queue_t *self,
                                         ntt_task_t *task, int *last);

NTT_EXPORT ntt_task_t *ntt_task_queue_pop_front_blocking(ntt_task_queue_t *self,
                                                         int *last);

EXTERN_STOP