#pragma once

#include "ntt/defs.h"
#include "ntt/impl/event_source.h"
#include "ntt/reader.h"

EXTERN_START

struct ntt_reader {
    ntt_event_source_t event;
    const ntt_reader_listener_tbl_t* listener_tbl;
    void* listener_ctx;
    uint8_t data[];
};

EXTERN_STOP
