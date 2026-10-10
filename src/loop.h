#pragma once

#include "./queue.h"
#include "./timer_service.h"
#include "combine.h"
#include "ntt/defs.h"
#include "ntt/impl/atomic.h"
#include "ntt/impl/epoll_event.h"
#include "ntt/queue.h"
#include "ntt/task.h"
#include <ntt/loop.h>

#include <pthread.h>
#include <signal.h>

EXTERN_START

typedef struct ntt_worker ntt_worker_t;

typedef struct ntt_loop_worker_ctx {
    pthread_t thread_id;
    ntt_worker_t* worker;
    ntt_loop_t* loop;
} ntt_loop_worker_ctx_t;

struct ntt_loop {
    int epoll_fd;

    atomic_size_t work_cnt;

    pthread_mutex_t mtx;
    pthread_cond_t cv;
    unsigned enter_cnt;
    unsigned width;

    ntt_combine_t combine;
    int combine_eventfd;
    ntt_epoll_event_t combine_event;

    /** The timer service every queue of this loop is armed on. */
    ntt_timer_service_t timer_service;

    // pthread_cond_t cnd;
    ntt_loop_worker_ctx_t workers[];
};

void ntt_loop_destroy(ntt_loop_t* self);

void ntt_loop_work_enter(ntt_loop_t* self);

void ntt_loop_work_leave(ntt_loop_t* self);

/**
 * @brief Send barrier task to the loop.
 *
 * Barrier task wakes up all workers. At task execution all previously
 * deleted fds from epoll will be drained.
 */
void ntt_loop_push_barrier(ntt_loop_t* self, ntt_task_t* task);

void ntt_loop_add_epoll_event(ntt_loop_t* self, ntt_epoll_event_t* epoll_event);

/**
 * @brief Add a descriptor owned by the loop itself to its epoll.
 *
 * Unlike ntt_loop_add_epoll_event this does not count as work of the loop:
 * such a descriptor must not keep the loop running on its own.
 */
void ntt_loop_add_internal_epoll_event(
    ntt_loop_t* self,
    int fd,
    struct epoll_event* event);

/**
 * @brief The timer service of this loop.
 */
static inline ntt_timer_service_t* ntt_loop_timer_service(ntt_loop_t* self)
{
    return &self->timer_service;
}

/**
 * @brief The combine of this loop.
 */
ntt_combine_t* ntt_loop_combine(ntt_loop_t* self);

void ntt_loop_del_epoll_event(ntt_loop_t* self, ntt_epoll_event_t* epoll_event);

ntt_loop_t* ntt_loop_self();

EXTERN_STOP
