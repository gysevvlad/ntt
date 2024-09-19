#pragma once

#include "ntt/defs.h"
#include "ntt/export.h"

EXTERN_START

/**
 * @brief Size of task payload.
 *
 * TODO: dynamic task payload size
 */
#define NTT_TASK_PAYLOAD_SIZE 48

/**
 * @brief Alignment of task payload.
 *
 * Usually allocators align 64-byte allocations to 16-byte. Struct ntt_task_t
 * has size of 64 bytes.
 *
 * TODO: over-aligned task payload data
 */
#define NTT_TASK_PAYLOAD_ALIGN 16

/**
 * @brief Task callback.
 */
typedef void(ntt_task_cb_t)(void *payload);

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
NTT_EXPORT ntt_task_t *ntt_make_task(ntt_task_cb_t *cb);

/**
 * @brief Do task.
 */
NTT_EXPORT void ntt_do_task(ntt_task_t *task);

/**
 * @brief Free task.
 */
NTT_EXPORT void ntt_free_task(void *task);

EXTERN_STOP
