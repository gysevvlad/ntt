#pragma once

#include "./node.h"
#include "ntt/defs.h"

EXTERN_START

#include <stddef.h>

typedef struct ntt_list ntt_list_t;

struct ntt_list {
    ntt_node_t sentinel;
};

/**
 * @brief Initialize list.
 */
static inline void ntt_list_init(ntt_list_t* self)
{
    ntt_node_set_next_relaxed(&self->sentinel, &self->sentinel);
    self->sentinel.prev = &self->sentinel;
}

/**
 * @brief Check that list is empty.
 *
 * @note Reads the head with acquire ordering, so it is safe to call without
 *       the lock that guards the list.
 */
static inline int ntt_list_empty(const ntt_list_t* self)
{
    return ntt_node_next_acquire(&self->sentinel) == &self->sentinel ? 1 : 0;
}

/**
 * @brief Push node to the and of list.
 *
 * @return Return 1 if list was empty before, otherwise 0.
 */
static inline int ntt_list_push_back(ntt_list_t* self, ntt_node_t* node)
{
    int rc = ntt_list_empty(self);

    ntt_node_set_next_relaxed(node, &self->sentinel);
    node->prev = self->sentinel.prev;

    // The store that makes the node reachable. When the list was empty this is
    // the head itself, which a reader may be loading without the lock, so it
    // publishes everything written before it.
    ntt_node_set_next_release(self->sentinel.prev, node);

    self->sentinel.prev = node;

    return rc;
}

/**
 * @brief Get pointer to the head node.
 *
 * @pre ntt_list_empty(self) != 1
 */
static inline ntt_node_t* ntt_list_front(const ntt_list_t* self)
{
    return ntt_node_next_acquire(&self->sentinel);
}

/**
 * @brief Forgot head node and return pointer to the next.
 *
 * @pre ntt_list_empty(self) != 1
 *
 * @return Pointer to the next node or NULL.
 */
static inline ntt_node_t* ntt_list_next(ntt_list_t* self)
{
    ntt_node_t* head = ntt_node_next_relaxed(&self->sentinel);
    ntt_node_t* next = ntt_node_next_relaxed(head);

    ntt_node_set_next_relaxed(&self->sentinel, next);
    next->prev = &self->sentinel;

    ntt_node_set_next_relaxed(head, NULL);

    return next != &self->sentinel ? next : NULL;
}

EXTERN_STOP
