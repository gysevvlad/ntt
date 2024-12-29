#pragma once

#include "ntt/event_source.h"
#include "ntt/impl/atomic.h"
#include "ntt/impl/event.h"

EXTERN_START

struct ntt_event_source {
    int fd;
    ntt_event_handler_tbl_t* event_handler_tbl;
    void* event_handler_ctx;
    int ec;
    ntt_pool_t* pool;
    atomic_size_t state;
    ntt_event_t event;
    int events_mask;
};

enum NTT_EVENT_SOURCE_STATE_MASKS {
    // clang-format off
    NTT_PENDING_IN_SOURCE_STATE     = 0b00000001,
    NTT_PENDING_OUT_SOURCE_STATE    = 0b00000010,
    NTT_PENDING_ERR_SOURCE_STATE    = 0b00000100,
    NTT_PENDING_HUP_SOURCE_STATE    = 0b00001000,
    NTT_PENDING_ANY_SOURCE_STATE    = 0b00001111,
    NTT_CANCELLED_SOURCE_STATE      = 0b00010000,
    NTT_BUSY_SOURCE_STATE           = 0b00100000,
    // clang-format on
};

void ntt_event_source_wakeup(void* self, uint32_t events);

EXTERN_STOP
