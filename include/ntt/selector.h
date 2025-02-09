#pragma once

#include "ntt/defs.h"
#include "ntt/ec.h"
#include "ntt/export.h"
#include "ntt/sigset.h"

EXTERN_START

typedef struct ntt_selector ntt_selector_t;

/**
 * @brief Allocate and construct a new selector.
 */
NTT_EXPORT ntt_selector_t* ntt_selector_create(
    ntt_ec_t* ec);

/**
 * @brief Run selector loop.
 */
NTT_EXPORT ntt_ec_t ntt_selector_run(
    ntt_selector_t* self,
    ntt_sigset_t* sigset);

/**
 * @brief Cleanup and free ntt_selector.
 */
NTT_EXPORT void ntt_selector_destroy(
    ntt_selector_t* self);

EXTERN_STOP
