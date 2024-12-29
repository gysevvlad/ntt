#pragma once

#include "ntt/defs.h"
#include "ntt/pool.h"

#include <stddef.h>
#include <stdint.h>

EXTERN_START

typedef struct ntt_reader ntt_reader_t;

typedef struct ntt_reader_listener_tbl {
    int (*on_data)(void* ctx, ntt_reader_t* reader, uint8_t* data, size_t size);
    void (*on_stop)(void* ctx, ntt_reader_t* reader, int ec);
} ntt_reader_listener_tbl_t;

NTT_EXPORT ntt_reader_t* ntt_reader_create(
    ntt_pool_t* pool,
    int fd,
    const ntt_reader_listener_tbl_t* listener_tbl,
    void* listener_ctx);

NTT_EXPORT void ntt_reader_start(
    ntt_reader_t* self);

NTT_EXPORT void ntt_reader_stop(
    ntt_reader_t* self);

NTT_EXPORT void ntt_reader_destroy(
    ntt_reader_t* self);

EXTERN_STOP
