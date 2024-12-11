#pragma once

#include "ntt/defs.h"
#include "ntt/export.h"
#include "ntt/pool.h"
#include "ntt/sockaddr.h"

EXTERN_START

typedef void(ntt_accept_cb)(void* ctx, int fd);
typedef void(ntt_stopped_cb)(void* ctx);
typedef struct ntt_accept_source ntt_accept_source_t;

NTT_EXPORT ntt_accept_source_t*
ntt_accept_source_create(ntt_pool_t* pool, ntt_accept_cb* accept_cb,
    void* accept_ctx, ntt_stopped_cb* cancel_cb,
    void* cancel_ctx, ntt_sockaddr_t* sockaddr);

NTT_EXPORT void ntt_accept_source_cancel(ntt_accept_source_t* self);

NTT_EXPORT void ntt_accept_source_acquire(ntt_accept_source_t* self);
NTT_EXPORT void ntt_accept_source_release(ntt_accept_source_t* self);

EXTERN_STOP
