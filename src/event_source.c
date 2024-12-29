#include "ntt/event_source.h"
#include "ntt/impl/event_source.h"
#include "ntt/impl/pool.h"
#include "ntt/pool.h"
#include "ntt/task.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/epoll.h>

static uint_fast32_t ntt_map_epoll_events_to_source_state(uint32_t events)
{
    uint_fast32_t v = 0;
    if (events & EPOLLIN) {
        v = v | NTT_PENDING_IN_SOURCE_STATE;
    }
    if (events & EPOLLOUT) {
        v = v | NTT_PENDING_OUT_SOURCE_STATE;
    }
    if (events & EPOLLERR) {
        v = v | NTT_PENDING_ERR_SOURCE_STATE;
    }
    if (events & EPOLLHUP) {
        v = v | NTT_PENDING_HUP_SOURCE_STATE;
    }
    if (events & EPOLLRDHUP) {
        // ignore EPOLLRDHUP
    }
    if (events & EPOLLPRI) {
        // ignore EPOLLPRI
    }
    return v;
}

static uint32_t ntt_map_source_state_to_epoll_events(uint_fast32_t state)
{
    uint32_t v = 0;
    if (state & NTT_PENDING_IN_SOURCE_STATE) {
        v = v | EPOLLIN;
    }
    if (state & NTT_PENDING_OUT_SOURCE_STATE) {
        v = v | EPOLLOUT;
    }
    if (state & NTT_PENDING_ERR_SOURCE_STATE) {
        v = v | EPOLLERR;
    }
    if (state & NTT_PENDING_HUP_SOURCE_STATE) {
        v = v | EPOLLHUP;
    }
    return v;
}

static uint32_t ntt_event_source_try_lock(ntt_event_source_t* self, uint32_t events)
{
    uint_fast32_t state = atomic_load_explicit(&self->state, memory_order_relaxed);

    do {
        if (state & NTT_BUSY_SOURCE_STATE) {
            // other thread working with fd, set pending events
            uint_fast32_t next_state = state | events;
            if (atomic_compare_exchange_weak(&self->state, &state, next_state)) {
                // notification about pending event setted, returns
                return 0;
            }
        } else {
            // we first thread on fd, try to take lock
            uint_fast32_t next_state = state | NTT_BUSY_SOURCE_STATE;
            if (atomic_compare_exchange_weak(&self->state, &state, next_state)) {
                // lock taken, continue
                assert((state & NTT_PENDING_ANY_SOURCE_STATE) == 0);
                return next_state | events;
            }
        }
    } while (1);
}

static uint32_t ntt_event_source_try_unlock(ntt_event_source_t* self)
{
    uint_fast32_t state = atomic_load_explicit(&self->state, memory_order_relaxed);

    do {
        if (state & NTT_PENDING_ANY_SOURCE_STATE) {
            // has any pending event, stay in busy state

            // try reset pending event
            uint_fast32_t next_state = state & ~NTT_PENDING_ANY_SOURCE_STATE;

            if (atomic_compare_exchange_weak(&self->state, &state, next_state)) {
                // do work cycle again

                // return previous state of pending events
                return state;
            }

        } else {
            // try release lock

            uint_fast32_t next_state = state & ~NTT_BUSY_SOURCE_STATE;

            if (atomic_compare_exchange_weak(&self->state, &state, next_state)) {
                // do nothing
                return 0;
            }
        }
    } while (1);
}

static void ntt_event_source_on_drain(void* ctx)
{
    ntt_event_source_t* self = *(ntt_event_source_t**)ctx;

    self->event_handler_tbl->on_del_cb(self, self->event_handler_ctx);
}

static void ntt_event_source_cancel_with_reason(ntt_event_source_t* self, int ec)
{
    uint_fast32_t state = atomic_load_explicit(&self->state, memory_order_relaxed);

    state = state & ~NTT_CANCELLED_SOURCE_STATE;

    do {

        uint_fast32_t next_state = state | NTT_CANCELLED_SOURCE_STATE;

        if (atomic_compare_exchange_weak(&self->state, &state, next_state)) {

            self->ec = ec;

            int rc = epoll_ctl(self->pool->epoll_fd, EPOLL_CTL_DEL, self->fd, NULL);
            assert(rc == 0);

            ntt_task_t* task = ntt_make_task(ntt_event_source_on_drain, free);
            *(void**)task = self;
            ntt_pool_post_barrier_task(self->pool, task);

            return;
        }

        if (state & NTT_CANCELLED_SOURCE_STATE) {
            return;
        }

    } while (1);
}

static int ntt_event_source_handle_state(ntt_event_source_t* self, uint32_t state)
{
    int events = 0;
    if (state & (NTT_PENDING_IN_SOURCE_STATE | NTT_PENDING_HUP_SOURCE_STATE | NTT_PENDING_ERR_SOURCE_STATE)) {
        if (self->events_mask & NTT_READ_EVENT) {
            events |= NTT_READ_EVENT;
        }
    }
    // TODO: check EPOLLHUP for write ready notification
    if (state & (NTT_PENDING_OUT_SOURCE_STATE | NTT_PENDING_ERR_SOURCE_STATE)) {
        if (self->events_mask & NTT_WRITE_EVENT) {
            events |= NTT_WRITE_EVENT;
        }
    }

    return events != 0
        ? self->event_handler_tbl->on_ready_cb(self, self->event_handler_ctx, events)
        : 0;
}

void ntt_event_source_wakeup(void* ctx, uint32_t events)
{
    ntt_event_source_t* self = ctx;

    uint_fast32_t state = ntt_event_source_try_lock(
        self,
        ntt_map_epoll_events_to_source_state(events));

    if (state == 0) {
        return;
    }

    do {
        int ec = ntt_event_source_handle_state(self, state);

        if (ec != 0) {
            ntt_event_source_cancel_with_reason(self, ec);

            // stay in busy state to reject incoming events from epoll
            return;
        }

        state = ntt_event_source_try_unlock(self);
    } while (state != 0);
}

ntt_event_source_t* ntt_event_source_create(
    ntt_pool_t* pool,
    ntt_event_handler_tbl_t* handler_tbl,
    void* handler_ctx,
    int fd,
    int events)
{
    assert((events & ~0b11) == 0);

    ntt_event_source_t* self = malloc(sizeof(ntt_event_source_t));

    if (self == NULL) {
        return self;
    }

    self->ec = 0;
    self->fd = fd;
    self->pool = ntt_pool_acquire(pool);
    self->state = 0;
    self->event_handler_tbl = handler_tbl;
    self->event_handler_ctx = handler_ctx;
    self->event.cb = ntt_event_source_wakeup;
    self->event.ctx = self;
    self->events_mask = events;

    return self;
}

void ntt_event_source_start(
    ntt_event_source_t* self)
{
    struct epoll_event event;
    event.data.ptr = &self->event;

    if (self->events_mask & NTT_READ_EVENT) {
        event.events = EPOLLIN | EPOLLET;
    }

    if (self->events_mask & NTT_WRITE_EVENT) {
        event.events = EPOLLOUT | EPOLLET;
    }

    int rc = epoll_ctl(self->pool->epoll_fd, EPOLL_CTL_ADD, self->fd, &event);
    if (rc == -1) {
        // TODO: schedule stop callback
        int ec = errno;
        fprintf(stderr, "[CRITICAL] failed to add %s to epoll: %s (%i)", self->event_handler_tbl->name, strerror(ec), ec);
        abort();
    }
}

void ntt_event_source_cancel(
    ntt_event_source_t* self)
{
    ntt_event_source_cancel_with_reason(self, 0);
}

void ntt_event_source_destroy(
    ntt_event_source_t* self)
{
    ntt_pool_release(self->pool);
    free(self);
}
