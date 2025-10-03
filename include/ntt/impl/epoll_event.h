#pragma once

#include <ntt/defs.h>
#include <ntt/impl/atomic.h>

#include <assert.h>
#include <stddef.h>

#include <sys/epoll.h>

EXTERN_START

typedef struct ntt_epoll_event ntt_epoll_event_t;
typedef struct ntt_epoll_event_vtbl ntt_epoll_event_vtbl_t;
typedef struct ntt_loop ntt_loop_t;

struct ntt_epoll_event_vtbl {
    const char* name;
    void (*ready)(ntt_epoll_event_t* self, void* ctx, uint32_t events);
    void (*cancelled)(ntt_epoll_event_t* self, void* ctx);
};

enum ntt_event_state {
    NTT_EVENT_STATE_LOCK_READ = 0b01,
    NTT_EVENT_STATE_WANT_READ = 0b10,
};

struct ntt_epoll_event {
    const ntt_epoll_event_vtbl_t* vtbl;
    void* ctx;
    uint32_t events;

    int fd;
    void* loop;
    atomic_uint_fast8_t state;
};

static inline void ntt_epoll_event_init(
    ntt_epoll_event_t* self,
    const ntt_epoll_event_vtbl_t* vtbl,
    void* ctx,
    uint32_t events)
{
    self->vtbl   = vtbl;
    self->ctx    = ctx;
    self->events = events;

    self->fd    = -1;
    self->loop  = NULL;
    self->state = 0b00;
}

static inline int ntt_event_state_lock_read(atomic_uint_fast8_t* self)
{
    uint_fast8_t old_state = atomic_load_explicit(self, memory_order_relaxed);

    for (;;) {
        if (ntt_unlikely((old_state & NTT_EVENT_STATE_LOCK_READ) != 0)) {
            // other thread is reading

            // set WANT_READ bit
            uint_fast8_t new_state = old_state;
            new_state |= NTT_EVENT_STATE_WANT_READ;

            if (atomic_compare_exchange_strong(self, &old_state, new_state)) {
                // other thread will do the reading again
                return 0;
            }
        } else {
            // current thread want to reading

            assert((old_state & NTT_EVENT_STATE_WANT_READ) == 0);

            // set LOCK_READ bit
            uint_fast8_t new_state = old_state;
            new_state |= NTT_EVENT_STATE_LOCK_READ;

            if (atomic_compare_exchange_strong(self, &old_state, new_state)) {
                // current thread take the read lock
                return 1;
            }
        }

        // try again
    };
}

static inline int ntt_event_state_unlock_read(atomic_uint_fast8_t* self)
{
    uint_fast8_t old_state = atomic_load_explicit(self, memory_order_relaxed);

    for (;;) {
        if (ntt_unlikely((old_state & NTT_EVENT_STATE_WANT_READ) != 0)) {
            // other thread got read notification

            assert((old_state & NTT_EVENT_STATE_LOCK_READ) != 0);

            // reset WANT_READ bit
            uint_fast8_t new_state = old_state;
            new_state &= ~NTT_EVENT_STATE_WANT_READ;

            if (atomic_compare_exchange_strong(self, &old_state, new_state)) {
                // current thread should read fd again
                return 0;
            }
        } else {
            // nothing to read

            // reset LOCK_READ bit
            uint_fast8_t new_state = old_state;
            new_state &= ~NTT_EVENT_STATE_LOCK_READ;

            if (atomic_compare_exchange_strong(self, &old_state, new_state)) {
                // current thread release the read lock
                return 1;
            }
        }

        // try again
    };
}

static inline void ntt_epoll_event_ready(struct epoll_event* event)
{
    ntt_epoll_event_t* self = (ntt_epoll_event_t*)event->data.ptr;
    self->vtbl->ready(self, self->ctx, event->events);
}

static inline void ntt_epoll_event_cancelled(ntt_epoll_event_t* self)
{
    self->vtbl->cancelled(self, self->ctx);
}

EXTERN_STOP
