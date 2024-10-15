#pragma once

#include "ntt/defs.h"
#include "ntt/export.h"
#include "ntt/task.h"

EXTERN_START

typedef struct ntt_pool2 ntt_pool2_t;

NTT_EXPORT ntt_pool2_t *ntt_pool2_create(unsigned short width);

NTT_EXPORT void ntt_pool2_wakeup_all(ntt_pool2_t *pool);

NTT_EXPORT void ntt_pool2_post_task(ntt_pool2_t *self, ntt_task_t *task);

NTT_EXPORT void ntt_pool2_send_task(ntt_pool2_t *pool, unsigned short idx,
                                    ntt_task_t *task);

NTT_EXPORT ntt_task_t *ntt_pool2_alloc_task(ntt_pool2_t *pool,
                                            ntt_task_cb_t *task_cb);

NTT_EXPORT void ntt_pool2_acquire(ntt_pool2_t *self);

NTT_EXPORT void ntt_pool2_release(ntt_pool2_t *self);

EXTERN_STOP