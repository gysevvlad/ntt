#include "./event.h"
#include "./loop.h"
#include "./queue.h"
#include "ntt/defs.h"
#include "ntt/event.h"
#include "ntt/impl/atomic.h"

#include <sys/epoll.h>

#include <assert.h>

/**
 * @brief State of one side of a descriptor.
 *
 * READY is a notification no handler has taken yet and BUSY says a handler of
 * this side is running. A notification that arrives while a handler runs is
 * just READY set again under BUSY.
 */
enum ntt_event_side_state {
    NTT_EVENT_SIDE_READY = 0b01,
    NTT_EVENT_SIDE_BUSY  = 0b10,
};

#define NTT_EVENT_SIDE_BITS 2
#define NTT_EVENT_READ_SHIFT 0
#define NTT_EVENT_WRITE_SHIFT NTT_EVENT_SIDE_BITS

/**
 * @brief The event is cancelled.
 *
 * The reference of the caller that started it is gone, and no side may be
 * claimed any more.
 */
#define NTT_EVENT_CLOSED (1u << (2 * NTT_EVENT_SIDE_BITS))

static unsigned ntt_event_side_shift(ntt_interest_t side)
{
    assert(side == NTT_INTEREST_READABLE || side == NTT_INTEREST_WRITABLE);

    return side == NTT_INTEREST_READABLE
        ? NTT_EVENT_READ_SHIFT
        : NTT_EVENT_WRITE_SHIFT;
}

static uint32_t ntt_event_side_state(ntt_interest_t sides, uint32_t bits)
{
    uint32_t state = 0;

    if ((sides & NTT_INTEREST_READABLE) != 0) {
        state = state | (bits << NTT_EVENT_READ_SHIFT);
    }

    if ((sides & NTT_INTEREST_WRITABLE) != 0) {
        state = state | (bits << NTT_EVENT_WRITE_SHIFT);
    }

    return state;
}

/**
 * @brief Translate what the kernel reported into the sides it concerns.
 *
 * An error or a hangup is reported whatever the registration asked for, and
 * both directions surface it: a read gives the error or the end of file, a
 * write gives the error. So they count as readiness of every side subscribed
 * to.
 */
static ntt_interest_t ntt_event_sides_of(uint32_t events, ntt_interest_t interest)
{
    ntt_interest_t sides = 0;

    if ((events & (EPOLLIN | EPOLLERR | EPOLLHUP)) != 0) {
        sides = sides | NTT_INTEREST_READABLE;
    }

    if ((events & (EPOLLOUT | EPOLLERR | EPOLLHUP)) != 0) {
        sides = sides | NTT_INTEREST_WRITABLE;
    }

    return sides & interest;
}

static int ntt_event_acquire_if_live(ntt_event_t* self)
{
    uint_least32_t refs = atomic_load_explicit(&self->refs, memory_order_relaxed);

    for (;;) {
        if (refs == 0) {
            return 0;
        }

        if (atomic_compare_exchange_weak_explicit(
                &self->refs,
                &refs,
                refs + 1,
                memory_order_acquire,
                memory_order_relaxed)) {
            return 1;
        }
    }
}

/**
 * @brief The handle of an event, rebuilt from the slot it lives in.
 *
 * Sound while a reference is held: the generation only moves when the slot
 * goes back to the slab.
 */
static ntt_event_handle_t ntt_event_handle_of(const ntt_event_t* self)
{
    return (ntt_event_handle_t)ntt_slab_index(self)
        | ((ntt_event_handle_t)ntt_slab_generation(self) << 32);
}

static ntt_event_t* ntt_event_of_task(void* payload)
{
    ntt_task_node_t* node = ntt_container_of(payload, ntt_task_node_t, payload);

    return ntt_container_of(node, ntt_event_t, task);
}

static void ntt_event_call(
    ntt_event_t* self,
    ntt_event_handle_t handle,
    ntt_interest_t side)
{
    if (side == NTT_INTEREST_READABLE) {
        self->vtbl->readable(handle, self->ctx, self->fd);
    } else {
        self->vtbl->writable(handle, self->ctx, self->fd);
    }
}

/**
 * @brief Take a side for this caller.
 *
 * @return 1 when the side is taken, 0 when another worker holds it, when there
 *         is nothing to report, or when the event is cancelled. Whoever holds
 *         the side sees the readiness published before this call and serves it.
 */
static int ntt_event_claim_side(ntt_event_t* self, ntt_interest_t side)
{
    unsigned shift = ntt_event_side_shift(side);

    uint32_t ready = (uint32_t)NTT_EVENT_SIDE_READY << shift;
    uint32_t busy  = (uint32_t)NTT_EVENT_SIDE_BUSY << shift;

    uint32_t state = atomic_load_explicit(&self->state, memory_order_relaxed);

    for (;;) {
        if ((state & ready) == 0
            || (state & (busy | NTT_EVENT_CLOSED)) != 0) {
            return 0;
        }

        if (atomic_compare_exchange_weak_explicit(
                &self->state,
                &state,
                (state & ~ready) | busy,
                memory_order_acquire,
                memory_order_relaxed)) {
            return 1;
        }
    }
}

/**
 * @brief Give a taken side up, unless a notification arrived meanwhile.
 *
 * @return 1 when the side stays taken because there is more to serve, 0 when
 *         it has been given up.
 */
static int ntt_event_leave_side(ntt_event_t* self, ntt_interest_t side)
{
    unsigned shift = ntt_event_side_shift(side);

    uint32_t ready = (uint32_t)NTT_EVENT_SIDE_READY << shift;
    uint32_t busy  = (uint32_t)NTT_EVENT_SIDE_BUSY << shift;

    uint32_t state = atomic_load_explicit(&self->state, memory_order_relaxed);

    for (;;) {
        int again = (state & ready) != 0 && (state & NTT_EVENT_CLOSED) == 0;

        uint32_t next = again
            ? (state & ~ready)
            : (state & ~(ready | busy));

        if (atomic_compare_exchange_weak_explicit(
                &self->state,
                &state,
                next,
                again ? memory_order_acquire : memory_order_release,
                memory_order_relaxed)) {
            return again;
        }
    }
}

/**
 * @brief Serve a taken side here and now, for as long as notifications come.
 */
static void ntt_event_serve_side(
    ntt_event_t* self,
    ntt_event_handle_t handle,
    ntt_interest_t side)
{
    do {
        ntt_event_call(self, handle, side);
    } while (ntt_event_leave_side(self, side));
}

static void ntt_event_release(ntt_loop_t* loop, ntt_event_t* self);
static void ntt_event_post_readable(ntt_event_t* self);

/**
 * @brief Body of the task that serves the readable side in the queue.
 *
 * NTT_TASK_DROP alone cannot happen: a queue is only destroyed once it is
 * empty.
 */
static void ntt_event_readable_task_svc(void* payload, ntt_task_action_t action)
{
    if (action != NTT_TASK_RUN) {
        return;
    }

    ntt_event_t* self = ntt_event_of_task(payload);

    ntt_event_call(self, ntt_event_handle_of(self), NTT_INTEREST_READABLE);
}

/**
 * @brief The queue is done with the node of the readable task.
 *
 * The side is held until here, and not until the end of the handler: the queue
 * reads the node after the callback has returned, so this is the first moment
 * the node may be handed over again. Owns the reference the dispatch gave.
 */
static void ntt_event_readable_task_free(void* ptr)
{
    ntt_event_t* self = ntt_container_of(
        (ntt_task_node_t*)ptr,
        ntt_event_t,
        task);

    if (ntt_event_leave_side(self, NTT_INTEREST_READABLE)) {
        ntt_event_post_readable(self);
        return;
    }

    ntt_event_release(ntt_queue_loop(self->queue), self);
}

static void ntt_event_post_readable(ntt_event_t* self)
{
    ntt_queue_push_task(
        self->queue,
        ntt_task_init(
            &self->task,
            ntt_event_readable_task_svc,
            ntt_event_readable_task_free));
}

static void ntt_event_cancelled_task_svc(void* payload, ntt_task_action_t action)
{
    if (action != NTT_TASK_RUN) {
        return;
    }

    ntt_event_t* self = ntt_event_of_task(payload);

    if (self->vtbl->cancelled != NULL) {
        self->vtbl->cancelled(self->ctx, self->fd);
    }
}

/**
 * @brief The queue is done with the node of the cancellation task.
 *
 * The slot goes back from here and not from the task body, because the queue
 * reads the node, which lives in the slot, after the body has returned.
 */
static void ntt_event_cancelled_task_free(void* ptr)
{
    ntt_event_t* self = ntt_container_of(
        (ntt_task_node_t*)ptr,
        ntt_event_t,
        task);

    ntt_queue_t* queue = self->queue;
    ntt_loop_t* loop   = ntt_queue_loop(queue);
    uint8_t flags      = self->flags;

    // Bumps the generation, so every handle of this event becomes stale.
    ntt_slab_give(ntt_loop_event_slab(loop), ntt_slab_index(self));
    ntt_queue_release(queue);

    // Last, since this is what lets the loop finish and take itself down.
    if ((flags & NTT_EVENT_WORK) != 0) {
        ntt_loop_work_leave(loop);
    }
}

/**
 * @brief Report the cancellation of an event the loop owns itself.
 */
static void ntt_event_teardown(ntt_loop_t* loop, ntt_event_t* self)
{
    uint32_t index = ntt_slab_index(self);
    uint8_t flags  = self->flags;

    if (self->vtbl->cancelled != NULL) {
        self->vtbl->cancelled(self->ctx, self->fd);
    }

    ntt_slab_give(ntt_loop_event_slab(loop), index);

    if ((flags & NTT_EVENT_WORK) != 0) {
        ntt_loop_work_leave(loop);
    }
}

static void ntt_event_release(ntt_loop_t* loop, ntt_event_t* self)
{
    // Acquire and release on the decrement itself, not a release plus a
    // standalone acquire fence: the fence is correct by the memory model but
    // opaque to the race detector, which then loses the edge between the last
    // handler leaving and the teardown this very call sets off.
    if (atomic_fetch_sub_explicit(&self->refs, 1, memory_order_acq_rel) != 1) {
        return;
    }

    if (self->queue == NULL) {
        ntt_event_teardown(loop, self);
        return;
    }

    // Nothing can reach the event any more, so no side holds the node: report
    // the cancellation where the owner of the descriptor lives.
    ntt_queue_push_task(
        self->queue,
        ntt_task_init(
            &self->task,
            ntt_event_cancelled_task_svc,
            ntt_event_cancelled_task_free));
}

static ntt_event_t* ntt_event_live(
    ntt_loop_t* loop,
    ntt_event_handle_t handle)
{
    ntt_event_t* self = ntt_slab_at(
        ntt_loop_event_slab(loop),
        ntt_event_handle_index(handle));

    // The storage of an event outlives the event itself, so take a reference
    // first and only then trust the generation.
    if (ntt_event_acquire_if_live(self) == 0) {
        return NULL;
    }

    if (ntt_slab_generation(self) != ntt_event_handle_generation(handle)) {
        ntt_event_release(loop, self);
        return NULL;
    }

    return self;
}

void ntt_event_open(
    ntt_loop_t* loop,
    ntt_queue_t* queue,
    int fd,
    uint32_t events,
    ntt_interest_t interest,
    const ntt_event_vtbl_t* vtbl,
    void* ctx,
    uint8_t flags,
    ntt_event_handle_t* handle)
{
    ntt_event_t* self = ntt_slab_take(ntt_loop_event_slab(loop));

    self->vtbl     = vtbl;
    self->ctx      = ctx;
    self->queue    = queue;
    self->fd       = fd;
    self->interest = (uint8_t)interest;
    self->flags    = flags;
    atomic_store_explicit(&self->state, 0, memory_order_relaxed);

    if (queue != NULL) {
        ntt_queue_acquire(queue);
    }

    ntt_event_handle_t id = ntt_event_handle_of(self);

    // Before anything can run a handler: a handler may cancel the event, and
    // the cancellation may free the very object this is written into.
    if (handle != NULL) {
        *handle = id;
    }

    // Publishes everything written above, including what the caller wrote
    // before getting here: a handler reaches the event through an acquiring
    // read of this counter.
    atomic_store_explicit(&self->refs, 1, memory_order_release);

    // Before the registration: the first notification may arrive from another
    // worker while this one is still here.
    if ((flags & NTT_EVENT_WORK) != 0) {
        ntt_loop_work_enter(loop);
    }

    struct epoll_event event;
    event.events   = events;
    event.data.u64 = id;
    ntt_loop_epoll_add(loop, fd, &event);
}

void ntt_event_dispatch(
    ntt_loop_t* loop,
    ntt_event_handle_t handle,
    uint32_t events)
{
    ntt_event_t* self = ntt_event_live(loop, handle);

    if (self == NULL) {
        return;
    }

    ntt_interest_t sides
        = ntt_event_sides_of(events, (ntt_interest_t)self->interest);

    if ((self->flags & NTT_EVENT_SERIAL) == 0) {
        if ((sides & NTT_INTEREST_READABLE) != 0) {
            ntt_event_call(self, handle, NTT_INTEREST_READABLE);
        }

        if ((sides & NTT_INTEREST_WRITABLE) != 0) {
            ntt_event_call(self, handle, NTT_INTEREST_WRITABLE);
        }

        ntt_event_release(loop, self);
        return;
    }

    if (sides == 0) {
        ntt_event_release(loop, self);
        return;
    }

    // Published before any side is claimed, so that whoever ends up serving a
    // side sees this notification, even if it is not this worker.
    atomic_fetch_or_explicit(
        &self->state,
        ntt_event_side_state(sides, NTT_EVENT_SIDE_READY),
        memory_order_release);

    // The writable side is served here: draining what is queued for sending is
    // the short side, and it has no business waiting behind a reader.
    if ((sides & NTT_INTEREST_WRITABLE) != 0
        && ntt_event_claim_side(self, NTT_INTEREST_WRITABLE)) {
        ntt_event_serve_side(self, handle, NTT_INTEREST_WRITABLE);
    }

    // The readable side goes to the queue of the event, which takes over this
    // reference.
    if ((sides & NTT_INTEREST_READABLE) != 0
        && ntt_event_claim_side(self, NTT_INTEREST_READABLE)) {
        ntt_event_post_readable(self);
        return;
    }

    ntt_event_release(loop, self);
}

void ntt_event_start(
    ntt_queue_t* queue,
    int fd,
    const ntt_event_vtbl_t* vtbl,
    void* ctx,
    ntt_interest_t interest,
    ntt_event_handle_t* handle)
{
    assert(queue != NULL);
    assert(interest != 0);
    assert(vtbl->cancelled != NULL);
    assert((interest & NTT_INTEREST_READABLE) == 0 || vtbl->readable != NULL);
    assert((interest & NTT_INTEREST_WRITABLE) == 0 || vtbl->writable != NULL);

    uint32_t events = 0;

    if ((interest & NTT_INTEREST_READABLE) != 0) {
        events = events | EPOLLIN | EPOLLET;
    }

    if ((interest & NTT_INTEREST_WRITABLE) != 0) {
        events = events | EPOLLOUT | EPOLLET;
    }

    ntt_event_open(
        ntt_queue_loop(queue),
        queue,
        fd,
        events,
        interest,
        vtbl,
        ctx,
        NTT_EVENT_WORK | NTT_EVENT_SERIAL,
        handle);
}

void ntt_event_cancel(ntt_loop_t* loop, ntt_event_handle_t handle)
{
    ntt_event_t* self = ntt_event_live(loop, handle);

    if (self == NULL) {
        return;
    }

    uint32_t state = (uint32_t)atomic_fetch_or_explicit(
        &self->state,
        NTT_EVENT_CLOSED,
        memory_order_acq_rel);

    // Whoever sets the bit owns the reference the caller of ntt_event_start
    // left behind, so cancelling twice drops it once.
    if ((state & NTT_EVENT_CLOSED) == 0) {
        ntt_loop_epoll_del(loop, self->fd);
        ntt_event_release(loop, self);
    }

    ntt_event_release(loop, self);
}
