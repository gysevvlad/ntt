#pragma once

#include "ntt/defs.h"

EXTERN_START

typedef struct ntt_pool ntt_pool_t;
typedef struct ntt_sockaddr ntt_sockaddr_t;

typedef struct ntt_connect_request ntt_connect_request_t;

typedef void(ntt_connect_cb_t)(
    void* ctx,
    int ec,
    int fd,
    ntt_sockaddr_t* local_addr,
    ntt_sockaddr_t* remote_addr);

NTT_EXPORT ntt_connect_request_t* ntt_connect_request_create();

NTT_EXPORT void ntt_connect_request_delete(
    ntt_connect_request_t* self);

NTT_EXPORT void ntt_connect_request_do(
    ntt_connect_request_t* self,
    ntt_pool_t* pool,
    ntt_sockaddr_t* addr,
    ntt_connect_cb_t* connect_cb,
    void* connect_ctx);

NTT_EXPORT ntt_connect_request_t* ntt_connect_request_cancel(
    ntt_connect_request_t* self);

EXTERN_STOP
