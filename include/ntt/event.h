#pragma once

#include "ntt/defs.h"
#include "ntt/pool.h"

EXTERN_START

typedef struct ntt_event_source ntt_event_source_t;

typedef enum ntt_event_type {
    NTT_READ_EVENT = 0b01,
    NTT_WRITE_EVENT = 0b10,
} ntt_event_type_t;

typedef int(ntt_event_ready_cb)(ntt_event_source_t* src, void* ctx, int events);
typedef void(ntt_event_canceled_cb)(ntt_event_source_t* src, void* ctx);

typedef struct ntt_event_vtbl {
    const char* name;
    ntt_event_ready_cb* ready_cb;
    ntt_event_canceled_cb* canceled_cb;
} ntt_event_vtbl_t;

NTT_EXPORT ntt_event_source_t* ntt_event_source_create(
    ntt_pool_t* pool,
    const ntt_event_vtbl_t* handler_tbl,
    void* handler_ctx,
    int fd,
    int events);

NTT_EXPORT void ntt_event_source_start(
    ntt_event_source_t* self);

NTT_EXPORT void ntt_event_source_cancel(
    ntt_event_source_t* self);

NTT_EXPORT void ntt_event_source_destroy(
    ntt_event_source_t* self);

EXTERN_STOP
