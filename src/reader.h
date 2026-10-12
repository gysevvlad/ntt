#pragma once

#include "ntt/defs.h"
#include "ntt/reader.h"

#include <ntt/event.h>

#include <stdint.h>

struct ntt_reader {
    const ntt_reader_vtbl_t* vtbl;
    void* ctx;

    ntt_loop_t* loop;
    ntt_event_handle_t event;

    /** What ended the reader, reported by on_cancel. */
    int ec;

    uint8_t data[1024];
};

void ntt_reader_init(
    ntt_reader_t* self,
    const ntt_reader_vtbl_t* vtbl,
    void* ctx);
