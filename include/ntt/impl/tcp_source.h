#pragma once

#include "ntt/defs.h"
#include "ntt/impl/event.h"
#include "ntt/impl/sockaddr.h"
#include "ntt/pool.h"
#include "ntt/sockaddr.h"
#include <pthread.h>
#include <stdatomic.h>

EXTERN_START

typedef struct ntt_tcp_source {
    ntt_pool_t* pool;
    int r_sock;
    int w_sock;
    ntt_sockaddr_t dst_sockaddr;
    ntt_event_t r_event;
    ntt_event_t w_event;

    atomic_uint_fast32_t state;

    pthread_mutex_t w_mutex;

    uint8_t* in_buf_data;
    size_t in_buf_len;
} ntt_tcp_source_t;

void ntt_tcp_source_construct(
    ntt_tcp_source_t* self,
    ntt_pool_t* pool,
    ntt_sockaddr_t* sockaddr);

void ntt_tcp_source_destruct(
    ntt_tcp_source_t* self);

EXTERN_STOP
