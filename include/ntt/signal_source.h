#pragma once

#include "ntt/defs.h"
#include "ntt/export.h"

EXTERN_START

typedef struct ntt_loop ntt_loop_t;
typedef struct ntt_signal_listener_vtbl ntt_signal_listener_vtbl_t;
typedef struct ntt_signal_source ntt_signal_source_t;

struct ntt_signal_listener_vtbl {
    char* name;
    void (*raised)(ntt_signal_source_t* loop, void* ctx, int signum);
    void (*stopped)(ntt_signal_source_t* loop, void* ctx);
};

NTT_EXPORT ntt_signal_source_t* ntt_signal_source_create(
    const ntt_signal_listener_vtbl_t* listener,
    void *ctx,
    int signum);

NTT_EXPORT void ntt_signal_source_start(
    ntt_signal_source_t* self,
    ntt_loop_t* loop);

NTT_EXPORT void ntt_signal_source_stop(
    ntt_signal_source_t* self);

NTT_EXPORT void ntt_signal_source_destroy(
    ntt_signal_source_t* self);

EXTERN_STOP
