#pragma once

#include "ntt/defs.h"
#include "ntt/export.h"
#include "ntt/impl/atomic.h"
#include "ntt/impl/event.h"
#include "ntt/selector.h"

EXTERN_START

#define NTT_SELECTOR_STATE_MASK (uint_fast32_t)0xFF000000
#define NTT_SELECTOR_COUNTER_MASK (uint_fast32_t)0x00FFFFFF

struct ntt_selector {
    int epoll_fd;
    atomic_uint_fast32_t state;
};

NTT_EXPORT ntt_ec_t ntt_selector_init(ntt_selector_t* self);

NTT_EXPORT void ntt_selector_deinit(ntt_selector_t* self);

EXTERN_STOP
