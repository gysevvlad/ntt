#pragma once

#include <ntt/defs.h>
#include <ntt/queue.h>

EXTERN_START

/**
 * @brief Ntt event loop.
 */
typedef struct ntt_loop ntt_loop_t;

/**
 * @brief Ntt event loop callbacks.
 */
typedef struct ntt_loop_cbs {
    void (*on_start)(void* ctx, ntt_queue_t* queue);
} ntt_loop_cbs_t;

/**
 * @brief Run ntt event loop.
 */
NTT_EXPORT int ntt_loop_run(ntt_loop_cbs_t cbs, void* ctx, unsigned width);

EXTERN_STOP
