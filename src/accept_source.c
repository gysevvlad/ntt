#include "ntt/accept_source.h"

#include "ntt/char_span.h"
#include "ntt/impl/accept_source.h"
#include "ntt/impl/event.h"
#include "ntt/impl/pool.h"
#include "ntt/impl/queue.h"
#include "ntt/impl/sockaddr.h"
#include "ntt/impl/socket.h"
#include "ntt/impl/task.h"
#include "ntt/pool.h"
#include "ntt/sockaddr.h"
#include "ntt/task.h"
#include "ntt/view.h"

#include <errno.h>
#include <netinet/in.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>

// int ntt_socket_make_from_sockaddr(int* self, ntt_sockaddr_t* sockaddr)
// {
//     assert(*self == -1);

//     int type = SOCK_CLOEXEC | SOCK_STREAM | SOCK_NONBLOCK;
//     int sock = socket(sockaddr->storage.ss_family, type, 0);
//     if (sock == -1) {
//         return errno;
//     }
//     *self = sock;
//     return 0;
// }

// int ntt_socket_set_reuse_addr(int* self)
// {
//     assert(*self != -1);

//     if (setsockopt(*self, SOL_SOCKET, SO_REUSEADDR, &(int) { 1 }, sizeof(int)) != 0) {
//         return errno;
//     }
//     return 0;
// }

// int ntt_socket_bind(int* self, ntt_sockaddr_t* sockaddr)
// {
//     int len = 0;
//     switch (sockaddr->storage.ss_family) {
//     case AF_INET:
//         len = sizeof(struct sockaddr_in);
//         break;
//     case AF_INET6:
//         len = sizeof(struct sockaddr_in6);
//         break;
//     case AF_UNIX:
//         len = sizeof(struct sockaddr_un);
//         break;
//     }

//     if (bind(*self, (struct sockaddr*)&sockaddr->storage, len) != 0) {
//         return errno;
//     }

//     return 0;
// }

// int ntt_socket_listen(int* self)
// {
//     if (listen(*self, SOMAXCONN) != 0) {
//         return errno;
//     }
//     return 0;
// }

// void ntt_socket_close(int* self)
// {
//     close(*self);
//     *self = -1;
// }

struct ntt_epoll_event_info {
    ntt_view_t view;
    uint32_t mask;
};

#define ntt_event_info_make(event)                           \
    {                                                        \
        .view = ntt_view_from_literal(#event), .mask = event \
    }

static struct ntt_epoll_event_info ntt_g_event_infos[] = {
    ntt_event_info_make(EPOLLIN),
    ntt_event_info_make(EPOLLOUT),
    ntt_event_info_make(EPOLLRDHUP),
    ntt_event_info_make(EPOLLPRI),
    ntt_event_info_make(EPOLLERR),
    ntt_event_info_make(EPOLLHUP),
};

size_t ntt_view_formatted_size(ntt_view_t* self) { return self->len; }

ntt_char_span_t ntt_view_format_to(ntt_view_t* self, ntt_char_span_t buffer)
{
    size_t len = self->len;
    if (buffer.len < len) {
        len = buffer.len;
    }
    memcpy(buffer.ptr, self->str, len);
    buffer.len -= len;
    buffer.ptr += len;
    return buffer;
}

size_t ntt_epoll_event_events_formatted_size(uint32_t events)
{
    size_t len = 0;
    size_t i;
    size_t cnt = 0;
    for (i = 0; i < sizeof(ntt_g_event_infos) / sizeof(ntt_g_event_infos[0]);
         ++i) {
        if (events & ntt_g_event_infos[i].mask) {
            if (cnt > 0) {
                len += 1; // '|'
            }
            len += ntt_view_formatted_size(&ntt_g_event_infos[i].view);
        }
    }
    return len;
}

ntt_char_span_t ntt_epoll_event_events_format_to(uint32_t events,
    ntt_char_span_t buffer)
{
    size_t len = 0;
    size_t i;
    size_t cnt = 0;
    for (i = 0; i < sizeof(ntt_g_event_infos) / sizeof(ntt_g_event_infos[0]);
         ++i) {
        if (events & ntt_g_event_infos[i].mask) {
            if (cnt > 0) {
                buffer = ntt_char_span_append_char(buffer, '|');
            }
            buffer = ntt_view_format_to(&ntt_g_event_infos[i].view, buffer);
        }
    }
    return buffer;
}

// void ntt_accept_svc2(void* ctx, uint32_t events)
// {
//     ntt_accept_source_t* self = ctx;
//     char buffer[1024];
//     ntt_char_span_t span = ntt_char_span_from_len_and_ptr(1023, buffer);
//     span = ntt_epoll_event_events_format_to(events, span);
//     span.ptr[span.len] = '\0';
//     printf("accept_source wakeup: %s\n", buffer);

//     int sock;

//     if (events & EPOLLHUP) {
//         // got shutdown event, no new wakeups occurs
//     }

// do_accept:
//     sock = accept4(self->sock, NULL, 0, SOCK_NONBLOCK | SOCK_CLOEXEC);
//     if (sock == -1) {
//         if (errno == EWOULDBLOCK || errno == EAGAIN) {
//             struct epoll_event event;
//             event.data.ptr = &self->event_cb;
//             event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
//             epoll_ctl(self->pool->epollfd, EPOLL_CTL_MOD, self->sock, &event);
//             return;
//         }
//         fprintf(stderr, "accept4 failed: %s (%i)", strerror(errno), errno);
//         // TODO: ...
//         abort();
//     }
//     self->cb(self->ctx, sock);
//     goto do_accept;
// }

enum NTT_SOURCE_STATE_MASKS {
    // clang-format off
    NTT_PENDING_IN_SOURCE_STATE  = 0b0000001,
    NTT_PENDING_OUT_SOURCE_STATE = 0b0000010,
    NTT_PENDING_ERR_SOURCE_STATE = 0b0000100,
    NTT_PENDING_HUP_SOURCE_STATE = 0b0001000,
    NTT_PENDING_ANY_SOURCE_STATE = 0b0001111,
    NTT_PENDING_END_SOURCE_STATE = NTT_PENDING_ERR_SOURCE_STATE | NTT_PENDING_HUP_SOURCE_STATE,
    NTT_BUSY_SOURCE_STATE        = 0b1000000,
    // clang-format on
};

uint_fast32_t ntt_map_epoll_events_to_source_state(uint32_t events)
{
    size_t v = 0;
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

atomic_size_t ntt_accept_source_wakeups_count = 0;

void ntt_accept_svc(void* ctx, uint32_t events)
{
    atomic_fetch_add(&ntt_accept_source_wakeups_count, 1);

    ntt_accept_source_t* self = ctx;
    // char buffer[1024];
    // ntt_char_span_t span = ntt_char_span_from_len_and_ptr(1023, buffer);
    // span = ntt_epoll_event_events_format_to(events, span);
    // span.ptr[span.len] = '\0';
    // printf("accept_source wakeup: %s\n", buffer);

    uint_fast32_t state = atomic_load_explicit(&self->state, memory_order_relaxed);

    do {
        if (state & NTT_BUSY_SOURCE_STATE) {
            // other thread working with fd, set pending events
            uint_fast32_t next_state = state | ntt_map_epoll_events_to_source_state(events);
            if (atomic_compare_exchange_weak(&self->state, &state, next_state)) {
                // notification about pending event setted, returns
                return;
            }
        } else {
            // we first thread on fd, try to take lock
            uint_fast32_t next_state = state | NTT_BUSY_SOURCE_STATE;
            if (atomic_compare_exchange_weak(&self->state, &state, next_state)) {
                // lock taken, continue
                break;
            }
        }
    } while (1);

    int sock;
    int ec;

do_accept:
    sock = accept4(self->sock, NULL, 0, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (sock == -1) {
        ec = errno;
        if (ec == EAGAIN || ec == EWOULDBLOCK) {
            uint_fast32_t state = atomic_load_explicit(&self->state, memory_order_relaxed);
            do {
                if (state & NTT_PENDING_ANY_SOURCE_STATE) {
                    // has any pending event

                    // try reset pending event
                    uint_fast32_t next_state = state & ~NTT_PENDING_ANY_SOURCE_STATE;

                    if (atomic_compare_exchange_weak(&self->state, &state, next_state)) {
                        // do work cycle again
                        goto do_accept;
                    }
                } else {
                    // try release lock

                    uint_fast32_t next_state = state & ~NTT_BUSY_SOURCE_STATE;

                    if (atomic_compare_exchange_weak(&self->state, &state, next_state)) {
                        return;
                    }
                }
            } while (1);
        } else if (ec == EINTR) {
            goto do_accept;
        } else {
            // TODO: how to handle errors?
            fprintf(stderr, "[critical][accept_source] got unexpected ec from accept4: %s (%i)\n", strerror(ec), ec);
            abort();
        }
    }
    self->accept_cb(self->accept_ctx, sock);
    goto do_accept;
}

void ntt_accept_source_init(
    ntt_accept_source_t* self,
    ntt_pool_t* pool,
    ntt_sockaddr_t* sockaddr)
{
    self->pool = pool;
    self->sockaddr = *sockaddr; // TODO: do it later via ntt_accept_source_set_sockaddr
    self->sock = -1;
    self->event_cb.cb = ntt_accept_svc;
    self->event_cb.ctx = self; // TODO: should I pin object in memory before start?
    self->state = 0; // TODO: describe source states
    self->accept_cb = NULL;
    self->accept_ctx = NULL;
    self->stopped_cb = NULL;
    self->stopped_ctx = NULL;
}

ntt_accept_source_t* ntt_accept_source_create(
    ntt_pool_t* pool,
    ntt_sockaddr_t* sockaddr)
{
    ntt_accept_source_t* self = malloc(sizeof(ntt_accept_source_t));
    if (self != NULL) {
        ntt_accept_source_init(self, pool, sockaddr);
    }
    return self;
}

int ntt_accept_source_setup(
    ntt_accept_source_t* self)
{
    int ec = 0;

    ec = ntt_socket_make_from_sockaddr(&self->sock, &self->sockaddr);

    if (ec != 0) {
        return ec;
    }

    ec = ntt_socket_set_reuse_addr(&self->sock);

    if (ec != 0) {
        ntt_socket_close(&self->sock);
        return ec;
    }

    ec = ntt_socket_bind(&self->sock, &self->sockaddr);

    if (ec != 0) {
        ntt_socket_close(&self->sock);
        return ec;
    }

    ec = ntt_socket_listen(&self->sock);

    if (ec != 0) {
        ntt_socket_close(&self->sock);
        return ec;
    }

    struct epoll_event event;
    event.data.ptr = &self->event_cb;
    event.events = EPOLLIN | EPOLLET;

    int rc = epoll_ctl(self->pool->epoll_fd, EPOLL_CTL_ADD, self->sock, &event);
    if (rc == -1) {
        ec = errno;
        ntt_socket_close(&self->sock);
        return ec;
    }

    return 0;
}

void ntt_accept_source_start(
    ntt_accept_source_t* self,
    ntt_accept_cb* accept_cb, void* accept_ctx,
    ntt_stopped_cb* stopped_cb, void* stopped_ctx)
{
    assert(self->accept_cb == NULL);
    assert(self->accept_ctx == NULL);
    assert(self->stopped_cb == NULL);
    assert(self->stopped_ctx == NULL);

    self->accept_cb = accept_cb;
    self->accept_ctx = accept_ctx;
    self->stopped_cb = stopped_cb;
    self->stopped_ctx = stopped_ctx;

    int ec = ntt_accept_source_setup(self);

    if (ec != 0) {
        self->accept_cb = NULL;
        self->accept_ctx = NULL;
        self->stopped_cb = NULL;
        self->stopped_ctx = NULL;

        stopped_cb(ec, stopped_ctx);
    }
}

void ntt_accept_source_on_cancel_impl(void* payload)
{
    ntt_accept_source_t* self = *(ntt_accept_source_t**)payload;

    ntt_socket_close(&self->sock);

    self->stopped_cb(0, self->stopped_ctx);
}

void ntt_accept_source_stop(ntt_accept_source_t* self)
{
    // TODO: check actual state

    int rc;

    rc = epoll_ctl(self->pool->epoll_fd, EPOLL_CTL_DEL, self->sock, NULL);
    if (rc == -1) {
        int ec = errno;
        // TODO: handle error, do stop procedure
        printf("failed to del accept source from epoll: %s (%i)", strerror(ec), ec);
        exit(-1);
    }

    ntt_task_t* task = ntt_make_task(ntt_accept_source_on_cancel_impl, NULL);
    *(ntt_accept_source_t**)task = self;
    ntt_pool_post_barrier_task(self->pool, task);
}

void ntt_accept_source_destroy(
    ntt_accept_source_t* self)
{
    // TODO: ...
}

void ntt_accept_source_delete(
    ntt_accept_source_t* self)
{
    ntt_accept_source_destroy(self);
    free(self);
}

size_t ntt_accept_source_get_wakeups_count()
{
    return atomic_load_explicit(&ntt_accept_source_wakeups_count, memory_order_relaxed);
}
