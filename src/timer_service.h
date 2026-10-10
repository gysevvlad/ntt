#pragma once

#include "./queue_heap.h"
#include "ntt/defs.h"
#include "ntt/impl/epoll_event.h"
#include "ntt/timer.h" // IWYU pragma: export

EXTERN_START

#include <pthread.h>
#include <stdint.h>

typedef struct ntt_loop ntt_loop_t;
typedef struct ntt_queue ntt_queue_t;

/**
 * @brief Timer service: one timerfd and the set of queues armed on it.
 *
 * Internal for now. Creating one registers a timerfd in the epoll of the
 * loop, and there is no way yet to take a descriptor out of the epoll while
 * workers are running: that needs the barrier drain which
 * ntt_loop_del_epoll_event is still missing. So a service can only be
 * released together with the loop, which is not something to put in the
 * public API.
 */
typedef struct ntt_timer_service ntt_timer_service_t;

/**
 * @brief Timer service.
 *
 * Owns one timerfd registered in the epoll of the loop and a heap of the
 * queues that have timers armed, keyed by their earliest deadline.
 *
 * Everything but the loop pointer is guarded by the lock. The timerfd is
 * re-armed only when the earliest deadline becomes earlier than the one it is
 * armed at, so a cancellation never costs a system call: the stale arming
 * just produces one spurious wakeup.
 */
struct ntt_timer_service {
    pthread_spinlock_t lock;

    ntt_queue_heap_t queues;

    /** Deadline the timerfd is armed at, UINT64_MAX when it is not. */
    uint64_t armed;

    int fd;

    ntt_loop_t* loop;
    ntt_epoll_event_t event;
};

/**
 * @brief Initialize the service and register its timerfd in the epoll of the loop.
 */
void ntt_timer_service_init(ntt_timer_service_t* self, ntt_loop_t* loop);

/**
 * @brief Close the timerfd and release the service.
 *
 * @pre No queue is registered, which holds once every worker has joined.
 */
void ntt_timer_service_deinit(ntt_timer_service_t* self);

/**
 * @brief The loop that owns this service.
 */
static inline ntt_loop_t* ntt_timer_service_loop(ntt_timer_service_t* self)
{
    return self->loop;
}

/**
 * @brief Ask the service to wake the queue no later than the deadline.
 *
 * Registers the queue, or moves its deadline earlier if it is registered with
 * a later one. Acquires the queue when it was not registered.
 */
void ntt_timer_service_register(
    ntt_timer_service_t* self,
    ntt_queue_t* queue,
    uint64_t deadline);

/**
 * @brief Drop the registration of a queue that has no timers left.
 */
void ntt_timer_service_deregister(
    ntt_timer_service_t* self,
    ntt_queue_t* queue);

EXTERN_STOP
