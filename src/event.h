#pragma once

#include <ntt/event.h>
#include <ntt/impl/atomic.h>
#include <ntt/impl/epoll_event.h>

struct ntt_event {
    const ntt_event_vtbl_t* vtbl;
    void* ctx;
    ntt_interest_t interest;
    atomic_uint_fast8_t state;
    ntt_epoll_event_t raw_event;
};

void ntt_event_init(
    ntt_event_t* self,
    const ntt_event_vtbl_t* vtbl,
    void* ctx,
    ntt_interest_t interest);
