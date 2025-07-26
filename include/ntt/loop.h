#pragma once

#include <ntt/defs.h>

EXTERN_START

typedef struct ntt_loop ntt_loop_t;

typedef struct ntt_loop_vptr {
    const char* name;
    void (*started)(ntt_loop_t* loop, void* ctx);
} ntt_loop_vptr_t;

NTT_EXPORT int ntt_loop_svc(const ntt_loop_vptr_t* cbs, void* ctx, unsigned width);

EXTERN_STOP
