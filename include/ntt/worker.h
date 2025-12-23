#pragma once

#include "ntt/defs.h"
#include "ntt/export.h"
#include "ntt/sigset.h"
#include "ntt/task.h"

EXTERN_START

typedef struct ntt_worker ntt_worker_t;
typedef struct ntt_worker_cbs ntt_worker_cbs_t;

struct ntt_worker_cbs {
    void (*enter_cb)(void* ctx, ntt_worker_t* worker);
    void (*svc_cb)(void* ctx, ntt_sigset_t* sigset);
    void (*leave_cb)(void* cxt);
};

NTT_EXPORT ntt_worker_t* ntt_worker_acquire(
    ntt_worker_t* self);

NTT_EXPORT void ntt_worker_release(
    ntt_worker_t* self);

NTT_EXPORT int ntt_worker_svc(
    ntt_worker_cbs_t cbs,
    void* ctx);

NTT_EXPORT void ntt_worker_post_task(
    ntt_worker_t* self,
    ntt_task_t* task);

EXTERN_STOP
