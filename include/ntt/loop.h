#pragma once

#include "ntt/task.h"

#include <ntt/defs.h>
#include <ntt/queue.h>

EXTERN_START

#define NTT_MAX_LOOP_WIDTH 8

/**
 * @brief Ntt event loop.
 */
typedef struct ntt_loop ntt_loop_t;

/**
 * @brief Run the ntt event loop.
 */
NTT_EXPORT int ntt_loop_run(ntt_task_t* task, unsigned width);

EXTERN_STOP
