#pragma once

#include "ntt/accept_source.h"

#include "ntt/defs.h"
#include "ntt/impl/atomic.h"
#include "ntt/impl/event.h"
#include "ntt/impl/task.h"
#include "ntt/pool.h"
#include "ntt/sockaddr.h"
#include "ntt/task.h"

EXTERN_START

struct ntt_accept_source {
    int sock;
    ntt_event_t cb_event;
    ntt_pool_t* pool;
    ntt_accept_cb* cb;
    atomic_size_t state;
    ntt_sockaddr_t* sockaddr;
    int shutdown;
    void* ctx;
    atomic_size_t refs;
    ntt_stopped_cb* cancel_cb;
    void* cancel_ctx;

    struct ntt_task_node flush_task;
};

EXTERN_STOP
