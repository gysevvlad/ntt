#pragma once

#include "./queue_heap.h"
#include "ntt/defs.h"
#include "./event.h"
#include "ntt/timer.h" // IWYU pragma: export

EXTERN_START

#include <pthread.h>
#include <stdint.h>

typedef struct ntt_loop ntt_loop_t;
typedef struct ntt_queue ntt_queue_t;

/**
 * @brief Timer service: one timerfd and the set of queues armed on it.
 *
 * Internal for now: a service is only released together with the loop, so
 * there is nothing to hand out yet. Independent scheduling groups are what
 * several services would be for.
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
    ntt_event_handle_t event;
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
 * @brief Ask the service to wake the slot no later than the deadline.
 *
 * Adds an entry carrying the generation the queue registers with. Entries it
 * had before are already tombstones, and tombstones says how many of them the
 * heap has not been told about yet, so that it knows when to rebuild.
 *
 * Takes no reference: the queue keeps itself alive while it has timers armed,
 * and the service only ever upgrades a slot to a reference when it wakes it.
 *
 * Dropping a registration does not go through the service at all: the queue
 * bumps its own generation.
 */
void ntt_timer_service_register(
    ntt_timer_service_t* self,
    uint32_t index,
    uint32_t generation,
    uint64_t deadline,
    uint32_t tombstones);

EXTERN_STOP
