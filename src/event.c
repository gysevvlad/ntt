#include "ntt/event.h"

#include "ntt/defs.h"
#include "ntt/ec.h"
#include "ntt/impl/atomic.h"
#include "ntt/impl/epoll_event.h"
#include "ntt/impl/loop.h"
#include "ntt/impl/malloc.h"

#include <sys/epoll.h>

#include <assert.h>

struct ntt_event {
    const ntt_event_vtbl_t* vtbl;
    void* ctx;
    ntt_interest_t interest;
    atomic_uint_fast8_t state;
    ntt_epoll_event_t raw_event;
};

enum NTT_EVENT_STATE {
    NTT_EVENT_STATE_CREATED  = 0b00000000,
    NTT_EVENT_STATE_STARTED  = 0b00000001,
    NTT_EVENT_STATE_CANCELED = 0b00000010,
};

#define NTT_INVALID_HANDLE (-1)

static void ntt_event_svc(ntt_event_t* self)
{
    self->vtbl->ntt_event_svc_cb(self, self->ctx, self->interest);
}

static void ntt_event_stopped(ntt_event_t* self)
{
    ntt_ec_t ec = ntt_make_ok_ec();
    self->vtbl->ntt_event_stopped_cb(self, self->ctx, ec);
}

static void ntt_epoll_event_svc(ntt_epoll_event_t* self, void* ctx, uint32_t events)
{
    (void)events;

    if (ntt_event_state_lock_read(&self->state) == 1) {
        do {
            ntt_event_svc(ctx);
        } while (ntt_event_state_unlock_read(&self->state) == 0);
    }
}

enum ntt_event_action {
    NTT_EVENT_ACTION_START  = 0b01,
    NTT_EVENT_ACTION_CANCEL = 0b10,
};

typedef enum ntt_event_action ntt_event_action_t;

static void ntt_epoll_event_stopped(ntt_epoll_event_t* self, void* ctx)
{
    (void)self;

    ntt_event_stopped(ctx);
}

const static ntt_epoll_event_vtbl_t g_event_vtbl = {
    .name      = "ntt_event_t",
    .ready     = ntt_epoll_event_svc,
    .cancelled = ntt_epoll_event_stopped,
};

static void ntt_event_init(
    ntt_event_t* self,
    const ntt_event_vtbl_t* vtbl,
    void* ctx,
    int fd,
    ntt_interest_t interest)
{
    assert(interest != 0b00);

    uint32_t events = 0;

    if (interest & NTT_INTEREST_READABLE) {
        events = events | EPOLLIN | EPOLLET;
    }

    if (interest & NTT_INTEREST_WRITABLE) {
        events = events | EPOLLOUT | EPOLLET;
    }

    self->vtbl     = vtbl;
    self->ctx      = ctx;
    self->interest = interest;
    self->state    = NTT_EVENT_STATE_CREATED;
    ntt_epoll_event_init(&self->raw_event, &g_event_vtbl, self, fd, events);
}

ntt_event_t* ntt_event_create(
    const ntt_event_vtbl_t* vtbl,
    void* ctx,
    int fd,
    ntt_interest_t interest)
{
    ntt_event_t* self = ntt_malloc(sizeof(ntt_event_t));

    if (ntt_unlikely(self == NULL)) {
        return self;
    }

    ntt_event_init(self, vtbl, ctx, fd, interest);

    return self;
}

void ntt_event_start(ntt_event_t* self, ntt_loop_t* loop)
{
    ntt_loop_add_epoll_event(loop, &self->raw_event);

    uint8_t state = NTT_EVENT_STATE_CREATED;
    if (ntt_unlikely(!atomic_compare_exchange_strong(&self->state, &state, NTT_EVENT_STATE_STARTED))) {
        assert(state == NTT_EVENT_STATE_CANCELED);
        ntt_loop_del_epoll_event(self->raw_event.loop, &self->raw_event);
    }
}

void ntt_event_cancel(ntt_event_t* self)
{
    uint8_t state = atomic_load_explicit(&self->state, memory_order_relaxed);

    if (ntt_unlikely(state == NTT_EVENT_STATE_CANCELED)) {
        return; // event already canceled
    }

    if (ntt_unlikely(state == NTT_EVENT_STATE_CREATED)) {
        if (atomic_compare_exchange_strong(&self->state, &state, NTT_EVENT_STATE_CANCELED)) {
            return; // the cancellation will occur from ntt_event_start
        }
        if (ntt_unlikely(state == NTT_EVENT_STATE_CANCELED)) {
            return; // event already canceled
        }
    }

    assert(state == NTT_EVENT_STATE_STARTED);

    if (ntt_unlikely(!atomic_compare_exchange_strong(&self->state, &state, NTT_EVENT_STATE_CANCELED))) {
        return; // event already canceled
    }

    ntt_loop_del_epoll_event(self->raw_event.loop, &self->raw_event);
}

void ntt_event_delete(
    ntt_event_t* self)
{
    ntt_free(self);
}
