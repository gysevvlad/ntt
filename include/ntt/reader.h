#pragma once

#include <ntt/defs.h>
#include <ntt/loop.h>

#include <stddef.h>
#include <stdint.h>

EXTERN_START

typedef struct ntt_reader ntt_reader_t;

typedef struct ntt_reader_vtbl {
    const char* name;
    void (*on_data)(ntt_reader_t* self, void* ctx, uint8_t* data, size_t size);
    void (*on_cancel)(ntt_reader_t* self, void* ctx, int ec);
} ntt_reader_vtbl_t;

NTT_EXPORT ntt_reader_t* ntt_reader_create(
    const ntt_reader_vtbl_t* vtbl,
    void* ctx);

NTT_EXPORT void ntt_reader_start(
    ntt_reader_t* self,
    int fd,
    ntt_loop_t* loop);

NTT_EXPORT void ntt_reader_cancel(
    ntt_reader_t* self);

NTT_EXPORT void ntt_reader_delete(
    ntt_reader_t* self);

EXTERN_STOP
