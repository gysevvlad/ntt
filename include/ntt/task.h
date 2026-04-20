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
NTT_EXPORT ntt_task_t* ntt_task_create(
    ntt_task_cb_t* task_cb);

/**
 * @brief Execute task.
 */
NTT_EXPORT void ntt_task_svc(
    ntt_task_t* self);

/**
 * @brief Destroy task.
 */
NTT_EXPORT void ntt_task_destroy(
    ntt_task_t* self);

EXTERN_STOP
