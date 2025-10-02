#pragma once

#include "ntt/defs.h"
#include "ntt/ec.h"
#include "ntt/export.h"

EXTERN_START

typedef struct ntt_accept_source_vtbl ntt_accept_source_vtbl_t;
typedef struct ntt_accept_source ntt_accept_source_t;
typedef struct ntt_loop ntt_loop_t;
typedef struct ntt_sockaddr ntt_sockaddr_t;

struct ntt_accept_source_vtbl {
    char* name;
    void (*svc)(ntt_accept_source_t* accept_source, void* ctx, int sock);
    void (*stopped)(ntt_accept_source_t* accept_source, ntt_ec_t ec, void* ctx);
};

NTT_EXPORT ntt_accept_source_t* ntt_accept_source_create(
    const ntt_accept_source_vtbl_t* listener,
    void* ctx,
    ntt_sockaddr_t* sockaddr);

NTT_EXPORT void ntt_accept_source_start(
    ntt_accept_source_t* self,
    ntt_loop_t* loop);

NTT_EXPORT void ntt_accept_source_stop(
    ntt_accept_source_t* self);

NTT_EXPORT void ntt_accept_source_destroy(
    ntt_accept_source_t* self);

EXTERN_STOP
