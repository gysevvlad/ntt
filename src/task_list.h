#pragma once

#include "./list.h"
#include "./task.h"
#include "ntt/defs.h"

EXTERN_START

#include <assert.h>

struct ntt_task_list {
    ntt_list_t list;
};

/**
 * @brief Reperesents any list of tasks in ntt.
 */
typedef struct ntt_task_list ntt_task_list_t;

/**
 * @brief Initilize task list.
 */
static inline void ntt_task_list_init(ntt_task_list_t* self)
{
    ntt_list_init(&self->list);
}

/**
 * @brief Push back task to list.
 *
 * @pre ntt_task_unlinked(task) == 1
 */
static inline int ntt_task_list_push(ntt_task_list_t* self, ntt_task_t* task)
{
    assert(ntt_task_unlinked(task) == 1);

    ntt_task_node_t* task_node = ntt_container_of(task, ntt_task_node_t, payload);
    return ntt_list_push_back(&self->list, &task_node->node);
}

static inline int ntt_task_list_empty(ntt_task_list_t* self)
{
    return ntt_list_empty(&self->list);
}

/**
 * @brief Get first pointer to the first task in queue.
 *
 * @pre ntt_task_list_empty(self) == 0
 */
static inline ntt_task_t* ntt_task_list_front(ntt_task_list_t* self)
{
    assert(ntt_task_list_empty(self) == 0);

    ntt_node_t* node = ntt_list_front(&self->list);

    ntt_task_node_t* task_node = ntt_container_of(node, ntt_task_node_t, node);

    return &task_node->payload;
}

/**
 * @brief Forgot first task and return pointer to the next task.
 *
 * @pre ntt_task_list_empty(self) == 0
 */
static inline ntt_task_t* ntt_task_list_next(ntt_task_list_t* self)
{
    assert(ntt_task_list_empty(self) == 0);

    ntt_node_t* node = ntt_list_next(&self->list);
    if (node == NULL) {
        return NULL;
    }

    ntt_task_node_t* task_node = ntt_container_of(node, ntt_task_node_t, node);

    return &task_node->payload;
}

EXTERN_STOP
