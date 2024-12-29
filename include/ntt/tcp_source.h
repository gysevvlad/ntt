#pragma once

#include "ntt/defs.h"
#include "ntt/export.h"
#include "ntt/pool.h"

#include <stddef.h>
#include <stdint.h>

EXTERN_START

typedef struct ntt_tcp_source ntt_tcp_source_t;

typedef void(ntt_connected_cb_t)(int rc, void* ctx);
typedef void(ntt_recv_cb_t)(uint8_t* data, size_t len);
typedef void(ntt_stopped_cb_t)(int rc, void* ctx);

typedef struct ntt_tcp_session_handler {
    ntt_connected_cb_t* connected_cb;
    ntt_recv_cb_t* recv_cb;
    ntt_stopped_cb_t* stopped_cb;
} ntt_tcp_session_handler_t;

NTT_EXPORT ntt_tcp_source_t* ntt_tcp_source_create(
    ntt_pool_t* pool);

NTT_EXPORT void ntt_tcp_source_start(
    ntt_tcp_source_t* self,
    int connected_socket);

NTT_EXPORT void ntt_tcp_source_stop(
    ntt_tcp_source_t* self);

NTT_EXPORT void ntt_tcp_source_delete(
    ntt_tcp_source_t* self);

NTT_EXPORT void ntt_tcp_source_send(
    ntt_tcp_source_t* self,
    uint8_t* data,
    size_t len);

EXTERN_STOP
