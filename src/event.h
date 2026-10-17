#pragma once

#include "./slab.h"
#include <ntt/event.h>
#include <ntt/impl/atomic.h>
#include <ntt/queue.h>

#include <stdint.h>

typedef struct ntt_loop ntt_loop_t;

/** Sides a descriptor has, which index the queues and the task nodes. */
#define NTT_EVENT_SIDES 2

enum ntt_event_flags {
    /** Keeps the loop running while the event is started. */
    NTT_EVENT_WORK = 0b01,

    /** Handlers of this event never run concurrently. */
    NTT_EVENT_SERIAL = 0b10,
};

/**
 * @brief A descriptor registered in the epoll of a loop.
 *
 * Lives in the event slab of the loop, so the handle the kernel carries in
 * epoll_event.data.u64 stays safe to resolve after the event has stopped: the
 * generation of the slot tells a stale notification from a live one.
 *
 * Each side runs in its own queue, so its handler shares a context with the
 * timers and the tasks of that queue. Sides given different queues are served
 * at the same time, so a slow reader does not hold back a writer.
 *
 * The reference count is one for the caller that started the event plus one
 * for every handler in flight. The last one to leave reports the cancellation
 * and gives the slot back.
 *
 * The order of the fields keeps the whole thing within the one cache line the
 * slab would round up to anyway.
 */
typedef struct ntt_event {
    ntt_slab_head_t slab_head;

    const ntt_event_vtbl_t* vtbl;
    void* ctx;

    /**
     * @brief Queue each side is delivered in, by side index.
     *
     * NULL for the descriptors the loop owns itself, which are served on the
     * worker that got the notification.
     */
    ntt_queue_t* queues[NTT_EVENT_SIDES];

    atomic_uint_least32_t refs;

    /** Readiness and ownership of both sides, plus cancellation. */
    atomic_uint_least32_t state;

    int fd;

    /** Sides subscribed to, as ntt_interest_t. */
    uint8_t interest;

    uint8_t flags;
} ntt_event_t;

static inline uint32_t ntt_event_handle_index(ntt_event_handle_t handle)
{
    return (uint32_t)handle;
}

static inline uint32_t ntt_event_handle_generation(ntt_event_handle_t handle)
{
    return (uint32_t)(handle >> 32);
}

/**
 * @brief Register a descriptor and take the first reference.
 *
 * Internal entry point behind ntt_event_start, which also takes the epoll mask
 * as it is instead of deriving it from the interest. The loop uses it directly
 * for the descriptors it owns itself: those pass no queues, since they are
 * served on the worker, no NTT_EVENT_WORK, since they must not keep the loop
 * running, and may leave the cancelled callback NULL, since they are only
 * released with the loop.
 */
void ntt_event_open(
    ntt_loop_t* loop,
    const ntt_event_params_t* params,
    uint32_t events,
    uint8_t flags);

/**
 * @brief Run the handler of the event the notification carries.
 *
 * Does nothing when the handle is stale, which is what makes a cancellation
 * safe while notifications for the event may still be on their way.
 *
 * @param events Mask the kernel reported, which is what the handler is told
 *               about rather than the mask the event subscribed with.
 */
void ntt_event_dispatch(
    ntt_loop_t* loop,
    ntt_event_handle_t handle,
    uint32_t events);
