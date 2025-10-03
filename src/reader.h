#pragma once

#include "./event.h"
#include "ntt/defs.h"
#include "ntt/reader.h"

struct ntt_reader {
    const ntt_reader_vtbl_t* vtbl;
    void* ctx;
    ntt_event_t event;
    uint8_t data[1024];
};

void ntt_reader_init(
    ntt_reader_t* self,
    const ntt_reader_vtbl_t* vtbl,
    void* ctx);
