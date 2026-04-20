#pragma once

#include "ntt/defs.h"

EXTERN_START

#include <stddef.h>

typedef struct ntt_node ntt_node_t;

struct ntt_node {
    ntt_node_t* next;
    ntt_node_t* prev;
};

/**
 * @brief Init node.
 */
static inline void ntt_node_init(ntt_node_t* self)
{
    self->next = NULL;
}

/**
 * @brief Check that node unlinked.
 *
 * @return Returns 1 if task unlinked, otherwise 0.
 */
static inline int ntt_node_unlinked(ntt_node_t* self)
{
    return self->next != NULL ? 0 : 1;
}

EXTERN_STOP
