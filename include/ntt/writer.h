#pragma once

#include "ntt/defs.h"
#include "ntt/pool.h"

#include <stddef.h>
#include <stdint.h>

EXTERN_START

typedef struct ntt_writer ntt_writer_t;

typedef struct ntt_writer_listener_tbl {
    void (*on_stop)(void* ctx, ntt_writer_t* writer);
} ntt_writer_listener_tbl_t;

NTT_EXPORT ntt_writer_create(
    ntt_pool_t* pool,
    int fd,
    const ntt_writer_listener_tbl_t* listener_tbl,
    void* listener_ctx);

NTT_EXPORT void ntt_writer_start(
    ntt_writer_t* self);

NTT_EXPORT void ntt_writer_send(
    ntt_writer_t* self,
    const uint8_t* data,
    size_t size);

NTT_EXPORT void ntt_writer_stop(
    ntt_writer_t* self,
    const uint8_t* data,
    size_t size);

EXTERN_STOP
