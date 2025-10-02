#pragma once

#include "ntt/defs.h"
#include "ntt/pool.h"

EXTERN_START

typedef enum ntt_interest {
    NTT_INTEREST_READABLE = 0b01,
    NTT_INTEREST_WRITABLE = 0b10,
} ntt_interest_t;

typedef struct ntt_event ntt_event_t;

typedef int(ntt_on_ready_cb)(ntt_event_source_t* src, void* ctx, int events);
typedef int(ntt_on_write_ready_cb)(ntt_event_source_t* src, void* ctx);
typedef void(ntt_on_stopped_cb)(ntt_event_source_t* src, void* ctx);

typedef struct ntt_event_handler_tbl {
    const char* name;
    ntt_on_ready_cb* on_ready_cb;
    ntt_on_stopped_cb* on_del_cb;
} ntt_event_handler_tbl_t;

NTT_EXPORT ntt_event_source_t* ntt_event_source_create(
    ntt_pool_t* pool,
    const ntt_event_handler_tbl_t* handler_tbl,
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
