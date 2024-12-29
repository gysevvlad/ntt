#pragma once

#include "ntt/connect_request.h"
#include "ntt/defs.h"
#include "ntt/impl/event.h"
#include "ntt/impl/sockaddr.h"
#include "ntt/impl/atomic.h"
#include "ntt/pool.h"
#include "ntt/sockaddr.h"

EXTERN_START

struct ntt_connect_request {
    ntt_pool_t* pool;
    ntt_event_t event;
    int sock;
    ntt_connect_cb_t* connect_cb;
    void* connect_ctx;
    ntt_sockaddr_t *addr;
};

EXTERN_STOP
