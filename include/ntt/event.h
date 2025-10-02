#pragma once

#include "ntt/defs.h"
#include "ntt/ec.h"
#include "ntt/export.h"
#include "ntt/loop.h"

EXTERN_START

typedef enum ntt_interest {
    NTT_INTEREST_READABLE = 0b01,
    NTT_INTEREST_WRITABLE = 0b10,
} ntt_interest_t;

typedef struct ntt_event ntt_event_t;

typedef struct ntt_event_vtbl {
    const char* name;
    void (*ntt_event_svc_cb)(ntt_event_t* self, void* ctx, ntt_interest_t interest);
    void (*ntt_event_stopped_cb)(ntt_event_t* self, void* ctx, ntt_ec_t ec);
} ntt_event_vtbl_t;

NTT_EXPORT ntt_event_t* ntt_event_create(
    const ntt_event_vtbl_t* vtbl,
    void* ctx,
    int fd,
    ntt_interest_t interest);

/**
 * Start listening event.
 *
 * NB! This method should only be called once.
 */
NTT_EXPORT void ntt_event_start(
    ntt_event_t* self,
    ntt_loop_t* loop);

NTT_EXPORT void ntt_event_cancel(
    ntt_event_t* self);

NTT_EXPORT void ntt_event_delete(
    ntt_event_t* self);

EXTERN_STOP
