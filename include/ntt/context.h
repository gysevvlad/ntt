#pragma once

#include "ntt/defs.h"
#include "ntt/ec.h"

EXTERN_START

typedef struct ntt_context ntt_context_t;

NTT_EXPORT ntt_context_t* ntt_context_create();

NTT_EXPORT void ntt_context_set_diff_width(
    ntt_context_t* self,
    int diff);

NTT_EXPORT ntt_ec_t ntt_context_run(
    ntt_context_t* self);

NTT_EXPORT void ntt_context_work_enter(
    ntt_context_t* self);

NTT_EXPORT void ntt_context_work_leave(
    ntt_context_t* self);

EXTERN_STOP
