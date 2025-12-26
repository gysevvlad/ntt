#pragma once

#include "ntt/impl/node.h"

#include <stddef.h>

typedef struct ntt_list {
    ntt_node_t node;
} ntt_list_t;

static inline void ntt_list_init(ntt_list_t* queue)
{
    queue->node.next = &queue->node;
    queue->node.prev = &queue->node;
}

static inline int ntt_list_empty(ntt_list_t* queue)
{
    return queue->node.next == &queue->node;
}

static inline void ntt_list_push_back(ntt_list_t* queue, ntt_node_t* node)
{
    ntt_node_t* prev = &queue->node;
    ntt_node_t* next = queue->node.next;

    prev->next = node;
    next->prev = node;
    node->next = next;
    node->prev = prev;
}

static inline void ntt_list_swap(ntt_list_t* lhs, ntt_list_t* rhs)
{
    if (ntt_list_empty(lhs)) {
        if (ntt_list_empty(rhs)) {
            return;
        }

        ntt_node_t* rhs_next = rhs->node.next;
        ntt_node_t* rhs_prev = rhs->node.prev;

        lhs->node.prev = rhs_prev;
        lhs->node.next = rhs_next;

        rhs_next->prev = &lhs->node;
        rhs_prev->next = &lhs->node;

        rhs->node.next = &rhs->node;
        rhs->node.prev = &rhs->node;

        return;
    }

    ntt_list_t tmp = *lhs;

    ntt_node_t* lhs_prev = lhs->node.prev;
    ntt_node_t* lhs_next = lhs->node.next;

    ntt_node_t* rhs_prev = rhs->node.prev;
    ntt_node_t* rhs_next = rhs->node.next;

    lhs->node.next = rhs_next;
    lhs->node.prev = rhs_prev;

    rhs_next->prev = &lhs->node;
    rhs_prev->next = &lhs->node;

    rhs->node.next = lhs_next;
    rhs->node.prev = lhs_prev;

    lhs_next->prev = &rhs->node;
    lhs_prev->next = &rhs->node;
}

static inline ntt_node_t* ntt_list_front(ntt_list_t* list)
{
    return list->node.prev;
}

static inline ntt_node_t* ntt_list_pop_front(ntt_list_t* self, int* last)
{
    ntt_node_t* node = self->node.prev;
    ntt_node_t* prev = node->prev;
    ntt_node_t* next = node->next;

    prev->next = next;
    next->prev = prev;

    *last = ntt_list_empty(self);

    return node != &self->node ? node : NULL;
}
