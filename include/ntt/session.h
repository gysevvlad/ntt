#pragma once

#include "ntt/defs.h"
#include "ntt/pool.h"

#include <stddef.h>
#include <stdint.h>

EXTERN_START

typedef struct ntt_session ntt_session_t;

typedef struct ntt_session_listener_tbl {
    void (*on_start)(void* ctx, ntt_session_t* session, int ec);
    void (*on_data)(void* ctx, ntt_session_t* session, const uint8_t* data, size_t size);
    void (*on_stop)(void* ctx, ntt_session_t* session, int ec);
} ntt_session_listener_tbl_t;

void ntt_session_acquire(
    ntt_session_t* self);

void ntt_session_release(
    ntt_session_t* self);

void ntt_session_send(
    ntt_session_t* self,
    const uint8_t* data,
    size_t len);

void ntt_session_stop(
    ntt_session_t* self);

typedef struct ntt_session_backend ntt_session_backend_t;

NTT_EXPORT ntt_session_backend_t* ntt_session_backend_create(
    ntt_pool_t* pool,
    int sock,
    const ntt_session_listener_tbl_t* listener_tbl,
    void* listener_ctx);

NTT_EXPORT void ntt_session_backend_start(
    ntt_session_t* self);

NTT_EXPORT void ntt_session_backend_stop(
    ntt_session_t* self);

NTT_EXPORT void ntt_session_backend_destroy(
    ntt_session_t* self);

EXTERN_STOP
