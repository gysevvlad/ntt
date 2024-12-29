#pragma once

#include "ntt/defs.h"
#include "ntt/export.h"
#include "ntt/pool.h"
#include "ntt/sockaddr.h"

EXTERN_START

typedef void(ntt_accept_cb)(void* ctx, int fd);
typedef void(ntt_stopped_cb)(int ec, void* ctx);
typedef struct ntt_accept_source ntt_accept_source_t;

NTT_EXPORT ntt_accept_source_t* ntt_accept_source_create(
    ntt_pool_t* pool,
    ntt_sockaddr_t* sockaddr);

NTT_EXPORT void ntt_accept_source_start(
    ntt_accept_source_t* self,
    ntt_accept_cb* accept_cb, void* accept_ctx,
    ntt_stopped_cb* stopped_cb, void* stopped_ctx);

NTT_EXPORT void ntt_accept_source_stop(
    ntt_accept_source_t* self);

NTT_EXPORT void ntt_accept_source_destroy(
    ntt_accept_source_t* self);

NTT_EXPORT void ntt_accept_source_delete(
    ntt_accept_source_t* self);

NTT_EXPORT size_t ntt_accept_source_get_wakeups_count();

EXTERN_STOP

#include "impl/accept_source.h"
