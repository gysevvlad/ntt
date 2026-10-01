#pragma once

#include "ntt/defs.h"

EXTERN_START

#include <stdint.h>

/**
 * @brief Size of task payload.
 *
 * TODO: dynamic task payload size
 */
#define NTT_TASK_PAYLOAD_SIZE 32

/**
 * @brief Alignment of task payload.
 *
 * Usually allocators align 64-byte allocations to 16-byte. Struct ntt_task_t
 * has size of 64 bytes.
 *
 * TODO: over-aligned task payload data
 */
#define NTT_TASK_PAYLOAD_ALIGN 8

/**
 * @brief What a task callback is asked to do with the payload.
 */
typedef enum ntt_task_action {
    /** Run the payload. Does not destroy it. */
    NTT_TASK_RUN = 0,

    /** Destroy the payload without running it. */
    NTT_TASK_DROP = 1,
} ntt_task_action_t;

/**
 * @brief Task callback.
 *
 * Owns the lifetime of the payload: NTT_TASK_RUN runs it, NTT_TASK_DROP
 * destroys it. Storage of the task itself is owned by ntt_free_cb_t instead.
 */
typedef void(ntt_task_cb_t)(void* payload, ntt_task_action_t action);

/**
 * @brief Free callback.
 */
typedef void(ntt_free_cb_t)(void* ptr);

/**
 * @brief Opaque task structure.
 */
typedef void ntt_task_t;

/**
 * @brief Create new task.
 *
 * TODO: dynamic task payload size
 * TODO: over-aligned task payload data
 */
NTT_EXPORT ntt_task_t* ntt_task_create(
    ntt_task_cb_t* task_cb);

/**
 * @brief Execute task.
 *
 * Invokes the task callback with NTT_TASK_RUN. The payload stays alive, so
 * ntt_task_destroy must still be called afterwards.
 */
NTT_EXPORT void ntt_task_svc(
    ntt_task_t* self);

/**
 * @brief Destroy task.
 *
 * Invokes the task callback with NTT_TASK_DROP and then releases the storage
 * of the task. Called on every path, both after ntt_task_svc and instead of
 * it when the task is cancelled.
 */
NTT_EXPORT void ntt_task_destroy(
    ntt_task_t* self);

EXTERN_STOP
