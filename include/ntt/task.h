#pragma once

#include "ntt/defs.h"
#include "ntt/export.h"

#include <stdint.h>

EXTERN_START

/**
 * @brief Size of task payload.
 *
 * TODO: dynamic task payload size
 */
#define NTT_TASK_PAYLOAD_SIZE 38

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
 * @brief Task callback.
 */
typedef void(ntt_task_cb_t)(void* payload);

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
NTT_EXPORT ntt_task_t* ntt_make_task(ntt_task_cb_t* task_cb,
    ntt_free_cb_t* free_cb);

/**
 * @brief Do task.
 */
NTT_EXPORT void ntt_do_task(ntt_task_t* task);

/**
 * @brief Free task.
 */
NTT_EXPORT void ntt_free_task(void* task);

EXTERN_STOP
