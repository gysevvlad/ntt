#include "ntt/timer.h"
#include "./timer_impl.h"
#include "ntt/ec.h"
#include "ntt/impl/epoll_event.h"
#include "ntt/impl/malloc.h"

#include <bits/time.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/timerfd.h>
#include <time.h>

struct ntt_timer {
    int fd;
    ntt_epoll_event_t epoll_event;
};

void ntt_timer_epoll_event_ready(ntt_epoll_event_t* self, void* ctx, uint32_t events)
{
}

void ntt_timer_epoll_event_cancelled(ntt_epoll_event_t* self, void* ctx)
{
}

static const ntt_epoll_event_vtbl_t g_ntt_timer_epoll_event_vtbl = {
    .name      = "timer",
    .ready     = ntt_timer_epoll_event_ready,
    .cancelled = ntt_timer_epoll_event_cancelled,
};

int ntt_timer_init(ntt_timer_t* self)
{
    int steady_clock_id = CLOCK_MONOTONIC;
    int flags           = TFD_NONBLOCK | TFD_CLOEXEC;
    int fd              = timerfd_create(steady_clock_id, flags);

    if (fd == -1) {
        return errno;
    }

    ntt_epoll_event_init(
        &self->epoll_event,
        &g_ntt_timer_epoll_event_vtbl,
        self,
        EPOLLIN | EPOLLET);

    return 0;
}

int ntt_timer_create(ntt_timer_t** self)
{
    if (self == NULL) {
        return EINVAL;
    }

    void* m = ntt_malloc(sizeof(ntt_timer_t));

    if (m == NULL) {
        return ENOMEM;
    }

    int ec = ntt_timer_init(m);

    if (ec != 0) {
        ntt_free(m);
        return ec;
    }

    *self = m;
    return 0;
}

ntt_ec_t ntt_timer_schedule(ntt_timer_t* self, struct timespec delay)
{
    struct itimerspec new_value = {
        .it_value    = delay,
        .it_interval = { .tv_nsec = 0, .tv_sec = 0 },
    };

    // at this moment we want to reset previos handler that actually in pending state
    // all that we can to do, it's use expected gen. Or not? In delay mode, all that we can actually do -> cancel -> schedule
    // in abs mode we can use abs time as expected time of exeution

    int rc = timerfd_settime(self->fd, 0, &new_value, NULL);

    if (rc == -1) {
        return ntt_make_system_ec(errno);
    }

    return ntt_make_ok_ec();
}
