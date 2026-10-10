#pragma once

#include "ntt/defs.h"
#include "ntt/impl/atomic.h"

EXTERN_START

#include <stddef.h>
#include <stdint.h>

typedef struct ntt_node ntt_node_t;

/**
 * @brief Node of an intrusive list.
 *
 * The next pointer is atomic because the head of a list is read without the
 * lock: a push publishes through it with release, a reader takes it with
 * acquire. Accesses under the lock are relaxed.
 */
struct ntt_node {
    atomic_uintptr_t next;
    ntt_node_t* prev;
};

static inline ntt_node_t* ntt_node_next_relaxed(const ntt_node_t* self)
{
    return (ntt_node_t*)atomic_load_explicit(&self->next, memory_order_relaxed);
}

/**
 * @brief Read the next pointer and synchronize with whoever published it.
 */
static inline ntt_node_t* ntt_node_next_acquire(const ntt_node_t* self)
{
    return (ntt_node_t*)atomic_load_explicit(&self->next, memory_order_acquire);
}

static inline void ntt_node_set_next_relaxed(ntt_node_t* self, ntt_node_t* next)
{
    atomic_store_explicit(&self->next, (uintptr_t)next, memory_order_relaxed);
}

/**
 * @brief Publish the next pointer, making everything written before visible.
 */
static inline void ntt_node_set_next_release(ntt_node_t* self, ntt_node_t* next)
{
    atomic_store_explicit(&self->next, (uintptr_t)next, memory_order_release);
}

/**
 * @brief Init node.
 */
static inline void ntt_node_init(ntt_node_t* self)
{
    ntt_node_set_next_relaxed(self, NULL);
}

/**
 * @brief Check that node unlinked.
 *
 * @return Returns 1 if task unlinked, otherwise 0.
 */
static inline int ntt_node_unlinked(ntt_node_t* self)
{
    return ntt_node_next_relaxed(self) != NULL ? 0 : 1;
}

EXTERN_STOP
