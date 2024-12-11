#pragma once

#include "ntt/defs.h"
#include "ntt/impl/atomic.h"
#include "ntt/impl/task.h"
#include "ntt/impl/task_list.h"

#include <pthread.h>

#define SIGRTNTTUP SIGRTMIN

EXTERN_START

typedef struct ntt_thread ntt_thread_t;

struct ntt_thread {
    /// posix thread id
    pthread_t id;

    int epoll_fd; // listening epoll instance
    int active; // should continue waiting for epoll instance

    /// thread stop cb
    void (*complete_cb)(ntt_thread_t* thread, void* context);
    void* context;
    atomic_size_t stopped;

    /// thread task queue
    pthread_spinlock_t tasks_lock;
    int got_up; // SIGRTNTTUP was raised
    ntt_task_list_t tasks_lists; // thread tasks list
    ntt_task_node_t tasks_sentinel; // last thread task
};

void ntt_thread_init(ntt_thread_t* self,
    void (*complete_cb)(ntt_thread_t* thread, void* context),
    void* context, int epoll_fd);

/**
 * @brief Send task to thread.
 */
void ntt_thread_send_task(ntt_thread_t* self, ntt_task_t* task);

/**
 * @brief Send stop request to thread.
 *
 * @note This function should be called only once.
 */
void ntt_thread_send_stop(ntt_thread_t* self);

void ntt_thread_destroy(ntt_thread_t* self);

EXTERN_STOP
