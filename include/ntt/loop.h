#pragma once

#include <ntt/defs.h>

EXTERN_START

typedef struct ntt_loop ntt_loop_t;

typedef struct ntt_loop_cbs {
    void (*started)(ntt_loop_t* loop, void* ctx);
} ntt_loop_cbs_t;

static inline ntt_loop_cbs_t ntt_loop_cbs_make(
    void (*started)(ntt_loop_t* loop, void* ctx))
{
    ntt_loop_cbs_t cbs;
    cbs.started = started;
    return cbs;
}

NTT_EXPORT int ntt_loop_svc(ntt_loop_cbs_t cbs, void* ctx, unsigned width);

EXTERN_STOP
