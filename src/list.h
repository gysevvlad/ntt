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
    self->sentinel.next = &self->sentinel;
    self->sentinel.prev = &self->sentinel;
}

/**
 * @brief Check that list is empty.
 */
static inline int ntt_list_empty(ntt_list_t* self)
{
    return self->sentinel.next == &self->sentinel ? 1 : 0;
}

/**
 * @brief Push node to the and of list.
 *
 * @return Return 1 if list was empty before, otherwise 0.
 */
static inline int ntt_list_push_back(ntt_list_t* self, ntt_node_t* node)
{
    int rc = ntt_list_empty(self);

    node->next = &self->sentinel;
    node->prev = self->sentinel.prev;

    self->sentinel.prev->next = node;
    self->sentinel.prev       = node;

    return rc;
}

/**
 * @brief Get pointer to the head node.
 *
 * @pre ntt_list_empty(self) != 1
 */
static inline ntt_node_t* ntt_list_front(ntt_list_t* self)
{
    return self->sentinel.next;
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
    ntt_node_t* head = self->sentinel.next;

    self->sentinel.next = head->next;
    head->next->prev    = &self->sentinel;

    head->next = NULL;

    ntt_node_t* next = self->sentinel.next;

    return next != &self->sentinel ? next : NULL;
}

EXTERN_STOP
