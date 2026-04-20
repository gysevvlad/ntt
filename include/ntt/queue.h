#pragma once

#include "ntt/defs.h"
#include "ntt/task.h"

EXTERN_START

/**
 * @brief Representing queue of serail tasks.
 */
typedef struct ntt_queue ntt_queue_t;

/**
 * @brief Create ntt_queue_t in the current ntt_loop_t.
 *
 * @pre ntt_loop_current() != NULL
 */
NTT_EXPORT ntt_queue_t* ntt_queue_create();

/**
 * @brief Push task to the queue.
 *
 * @pre queue != NULL
 * @pre task != NULL && ntt_task_queued(task) == 0
 */
NTT_EXPORT void ntt_queue_push(
    ntt_queue_t* queue,
    ntt_task_t* task);

/**
 * @brief Returns the current queue.
 *
 * In ntt at one time only one queue can be executed in a thread.
 *
 * @return Pointer to the current queue or NULL if execution
 *         happens not in queue context.
 */
NTT_EXPORT ntt_queue_t* ntt_queue_self();

NTT_EXPORT ntt_queue_t* ntt_queue_acquire(
    ntt_queue_t* self);

NTT_EXPORT void ntt_queue_release(
    ntt_queue_t* self);

EXTERN_STOP
