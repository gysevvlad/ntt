#include "ntt/impl/selector.h"
#include "ntt/defs.h"
#include "ntt/ec.h"
#include "ntt/impl/event.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <threads.h>

#include <sys/epoll.h>

ntt_ec_t ntt_selector_init(ntt_selector_t* self)
{
    self->epoll_fd = epoll_create1(EPOLL_CLOEXEC);

    if ntt_unlikely (self->epoll_fd == -1) {
        return ntt_make_system_ec(errno);
    }

    atomic_init(&self->state, 0);

    return ntt_make_ok_ec();
}

void ntt_selector_deinit(ntt_selector_t* self)
{
    close(self->epoll_fd);
}

ntt_selector_t* ntt_selector_create(
    ntt_ec_t* ec)
{
    ntt_selector_t* self = malloc(sizeof(ntt_selector_t));

    if ntt_unlikely (self == NULL) {
        *ec = ntt_make_system_ec(ENOMEM);
        return NULL;
    }

    *ec = ntt_selector_init(self);

    if ntt_unlikely (ntt_ec_error(ec)) {
        free(self);
        return NULL;
    }

    return self;
}

void ntt_selector_epoll_event(struct epoll_event* ev)
{
    ntt_event_t* event = ev->data.ptr;
    event->cb(event->ctx, ev->events);
}

static ntt_ec_t ntt_selector_svc(
    ntt_selector_t* self,
    ntt_sigset_t* sigset)
{
    struct epoll_event event;

    int rc = epoll_pwait(
        self->epoll_fd,
        &event,
        1,
        -1, (sigset_t*)sigset);

    if ntt_unlikely (rc == -1) {
        return ntt_make_system_ec(errno);
    }

    if ntt_likely (rc == 1) {
        ntt_selector_epoll_event(&event);
    }

    return ntt_make_ok_ec();
}

ntt_ec_t ntt_selector_run(
    ntt_selector_t* self,
    ntt_sigset_t* sigset)
{
    ntt_ec_t error;

    do {
        error = ntt_selector_svc(self, sigset);
    } while (!ntt_ec_error(&error));

    return error;
}

void ntt_selector_destroy(
    ntt_selector_t* self)
{
    ntt_selector_deinit(self);
    free(self);
}
