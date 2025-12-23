#pragma once

#include <ntt/defs.h>

EXTERN_START

typedef struct ntt_loop ntt_loop_t;

typedef struct ntt_loop_cbs {
    void (*on_start)(ntt_loop_t* loop, void* ctx);
    void (*on_signal)(ntt_loop_t* loop, void* ctx, int signal);
} ntt_loop_cbs_t;

static inline ntt_loop_cbs_t ntt_loop_cbs_make(
    void (*started)(ntt_loop_t* loop, void* ctx),
    void (*on_signal)(ntt_loop_t* loop, void* ctx, int signal))
{
    ntt_loop_cbs_t cbs;
    cbs.on_start  = started;
    cbs.on_signal = on_signal;
    return cbs;
}

NTT_EXPORT int ntt_loop_svc(ntt_loop_cbs_t cbs, void* ctx, unsigned width);

NTT_EXPORT ntt_loop_t* ntt_loop_acquire(ntt_loop_t* self);
NTT_EXPORT void ntt_loop_release(ntt_loop_t* self);

EXTERN_STOP
