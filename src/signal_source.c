#include "ntt/signal_source.h"

#include "ntt/defs.h"
#include "ntt/impl/epoll_event.h"
#include "ntt/impl/loop.h"
#include "ntt/impl/malloc.h"
#include "ntt/signal.h"

#include <errno.h>

#include <sys/eventfd.h>

struct ntt_signal_source {
    const ntt_signal_listener_vtbl_t* listener;
    void* ctx;
    int signum;
    ntt_epoll_event_t event;
};

static void signal_source_eventfd_read(ntt_signal_source_t* self)
{
    eventfd_t value = 0;
    int rc = 0;
    int ec = 0;
    do {
        rc = eventfd_read(self->event.fd, &value);
        if (rc != 0) {
            ec = errno;
        } else {
            self->listener->raised(self, self->ctx, self->signum);
        }
    } while ((rc == 0) || (ec == EINTR));
}

static void signal_source_eventfd_ready(ntt_epoll_event_t* epoll_event, void* ctx, uint32_t events)
{
    ntt_signal_source_t* self = ctx;
    if ((events & EPOLLIN) != 0) {
        if (ntt_event_state_lock_read(&epoll_event->state)) {
            do {
                signal_source_eventfd_read(ctx);
            } while (!ntt_event_state_unlock_read(&epoll_event->state));
        }
    }
}

static void signal_source_eventfd_cancelled(ntt_epoll_event_t* epoll_event, void* ctx)
{
    (void)epoll_event;

    ntt_signal_source_t* self = ctx;
    if (ntt_likely(self->listener)) {
        self->listener->stopped(self, self->ctx);
    }
}

const static ntt_epoll_event_vtbl_t g_signal_source_event_vtbl = {
    .name = "signal_source_event",
    .ready = signal_source_eventfd_ready,
    .cancelled = signal_source_eventfd_cancelled,
};

ntt_signal_source_t* ntt_signal_source_create(
    const ntt_signal_listener_vtbl_t* listener,
    void* ctx,
    int signum)
{
    ntt_signal_source_t* self = ntt_malloc(sizeof(ntt_signal_source_t));
    if (self == NULL) {
        return NULL;
    }
    self->listener = listener;
    self->ctx = ctx;
    self->signum = signum;
    self->event.events = EPOLLIN | EPOLLET;
    self->event.ctx = self;
    self->event.fd = ntt_signal_get_eventfd(signum);
    self->event.loop = NULL;
    self->event.vtbl = &g_signal_source_event_vtbl;
    return self;
}

void ntt_signal_source_start(ntt_signal_source_t* self, ntt_loop_t* loop)
{
    ntt_loop_add_epoll_event(loop, &self->event);
}

void ntt_signal_source_stop(ntt_signal_source_t* self)
{
    ntt_loop_del_epoll_event(self->event.loop, &self->event);
}

void ntt_signal_source_destroy(ntt_signal_source_t* self)
{
    ntt_free(self);
}
