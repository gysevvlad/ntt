#pragma once

#include "ntt/accept_source.h"

#include "ntt/defs.h"
#include "ntt/impl/atomic.h"
#include "ntt/impl/event.h"
#include "ntt/impl/task.h"
#include "ntt/impl/sockaddr.h"
#include "ntt/pool.h"
#include "ntt/sockaddr.h"
#include "ntt/task.h"

EXTERN_START

struct ntt_accept_source {
    ntt_pool_t* pool;
    ntt_sockaddr_t sockaddr;
    int sock;
    ntt_event_t event_cb;
    atomic_uint_fast32_t state;
    ntt_accept_cb* accept_cb;
    void* accept_ctx;
    ntt_stopped_cb* stopped_cb;
    void* stopped_ctx;
};

void ntt_accept_source_init(
    ntt_accept_source_t* self,
    ntt_pool_t* pool,
    ntt_sockaddr_t* sockaddr);

EXTERN_STOP
