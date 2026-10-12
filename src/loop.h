#pragma once

#include "./queue.h"
#include "./slab.h"
#include "./timer_service.h"
#include "combine.h"
#include "ntt/defs.h"
#include "ntt/impl/atomic.h"
#include "./event.h"
#include "ntt/queue.h"
#include "ntt/task.h"
#include <ntt/loop.h>

#include <pthread.h>
#include <signal.h>
#include <sys/epoll.h>

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
    ntt_event_handle_t combine_event;

    /** Storage of every queue created in this loop. */
    ntt_slab_t queue_slab;

    /** Storage of every event registered in the epoll of this loop. */
    ntt_slab_t event_slab;

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

/**
 * @brief Add a descriptor to the epoll of the loop.
 *
 * The event layer owns every registration, so this is for it to call.
 */
void ntt_loop_epoll_add(ntt_loop_t* self, int fd, struct epoll_event* event);

/**
 * @brief Take a descriptor out of the epoll of the loop.
 *
 * Notifications for it may still be on their way to the workers, which is why
 * what they carry is a handle and not a pointer.
 */
void ntt_loop_epoll_del(ntt_loop_t* self, int fd);

/**
 * @brief The queue slab of this loop.
 */
static inline ntt_slab_t* ntt_loop_queue_slab(ntt_loop_t* self)
{
    return &self->queue_slab;
}

/**
 * @brief The event slab of this loop.
 */
static inline ntt_slab_t* ntt_loop_event_slab(ntt_loop_t* self)
{
    return &self->event_slab;
}

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

ntt_loop_t* ntt_loop_self();

EXTERN_STOP
