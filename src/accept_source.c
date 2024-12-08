#include "ntt/accept_source.h"

#include "ntt/char_span.h"
#include "ntt/impl/accept_source.h"
#include "ntt/impl/event.h"
#include "ntt/impl/pool.h"
#include "ntt/impl/queue.h"
#include "ntt/impl/sockaddr.h"
#include "ntt/sockaddr.h"
#include "ntt/view.h"

#include <asm-generic/errno.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>

int ntt_socket_make_from_sockaddr(int* self, ntt_sockaddr_t* sockaddr)
{
    int type = SOCK_CLOEXEC | SOCK_STREAM | SOCK_NONBLOCK;
    int sock = socket(sockaddr->storage.ss_family, type, 0);
    if (sock == -1) {
        return 0;
    }
    *self = sock;
    return 1;
}

int ntt_socket_bind(int* self, ntt_sockaddr_t* sockaddr)
{
    int len = 0;
    switch (sockaddr->storage.ss_family) {
    case AF_INET:
        len = sizeof(struct sockaddr_in);
        break;
    case AF_INET6:
        len = sizeof(struct sockaddr_in6);
        break;
    case AF_UNIX:
        len = sizeof(struct sockaddr_un);
        break;
    }
    if (bind(*self, (struct sockaddr*)&sockaddr->storage, len) != 0) {
        return 0;
    }
    return 1;
}

int ntt_socket_listen(int* self)
{
    if (listen(*self, SOMAXCONN) == -1) {
        return 0;
    }
    return 1;
}

void ntt_socket_close(int* self) { close(*self); }

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

void ntt_accept_svc2(void* ctx, uint32_t events)
{
    ntt_accept_source_t* self = ctx;
    char buffer[1024];
    ntt_char_span_t span = ntt_char_span_from_len_and_ptr(1023, buffer);
    span = ntt_epoll_event_events_format_to(events, span);
    span.ptr[span.len] = '\0';
    printf("accept_source wakeup: %s\n", buffer);

    int sock;

    if (events & EPOLLHUP) {
        // got shutdown event, no new wakeups occurs
    }

do_accept:
    sock = accept4(self->sock, NULL, 0, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (sock == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            struct epoll_event event;
            event.data.ptr = &self->cb_event;
            event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
            epoll_ctl(self->pool->epollfd, EPOLL_CTL_MOD, self->sock, &event);
            return;
        }
        fprintf(stderr, "accept4 failed: %s (%i)", strerror(errno), errno);
        // TODO: ...
        abort();
    }
    self->cb(self->ctx, sock);
    goto do_accept;
}

void ntt_accept_svc(void* ctx, uint32_t events)
{
    ntt_accept_source_t* self = ctx;
    char buffer[1024];
    ntt_char_span_t span = ntt_char_span_from_len_and_ptr(1023, buffer);
    span = ntt_epoll_event_events_format_to(events, span);
    span.ptr[span.len] = '\0';
    printf("accept_source wakeup: %s\n", buffer);

    int sock;

    do {
        size_t state = atomic_load_explicit(&self->state, memory_order_relaxed);
        assert((state & 0b1) != 1 && "got wakeup event but EPOLLHUP bit already set");
    } while (1);

    if (events & EPOLLHUP) {
        // got shutdown event, no new wakeups occurs
    }

    if (atomic_fetch_add(&self->state, 1) != 0) {
        // other thread in do_accept cycle
        return;
    }

do_accept:
    sock = accept4(self->sock, NULL, 0, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (sock == -1) {
        if (atomic_fetch_sub(&self->state, 1) != 1) {
            // check accept again
            goto do_accept;
        }
        if (events & EPOLLHUP) {
        }
    }
    self->cb(self->ctx, sock);
    goto do_accept;
}

ntt_accept_source_t*
ntt_accept_source_create(ntt_pool_t* pool, ntt_accept_cb* accept_cb, void* ctx,
    ntt_stopped_cb* cancel_cb, void* cancel_ctx,
    ntt_sockaddr_t* sockaddr)
{
    ntt_accept_source_t* self = malloc(sizeof(ntt_accept_source_t));

    self->refs = 1;

    if (!ntt_socket_make_from_sockaddr(&self->sock, sockaddr)) {
        free(self);
        return NULL;
    }

    if (setsockopt(self->sock, SOL_SOCKET, SO_REUSEADDR, &(int) { 1 }, sizeof(int)) < 0) {
        ntt_socket_close(&self->sock);
        free(self);
        return NULL;
    }

    if (!ntt_socket_bind(&self->sock, sockaddr)) {
        ntt_socket_close(&self->sock);
        free(self);
        return NULL;
    }

    if (!ntt_socket_listen(&self->sock)) {
        ntt_socket_close(&self->sock);
        free(self);
        return NULL;
    }

    self->cb_event.cb = ntt_accept_svc2;
    self->cb_event.ctx = self;

    self->cancel_cb = cancel_cb;
    self->cancel_ctx = cancel_ctx;

    self->cb = accept_cb;
    self->ctx = ctx;

    ntt_sockaddr_acquire(sockaddr);
    self->sockaddr = sockaddr;
    self->shutdown = 0;
    self->state = 0;

    struct epoll_event event;
    event.data.ptr = &self->cb_event;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

    if (epoll_ctl(pool->epollfd, EPOLL_CTL_ADD, self->sock, &event) == -1) {
        ntt_socket_close(&self->sock);
        ntt_sockaddr_release(self->sockaddr);
        free(self);
        return NULL;
    }

    return self;
}

void ntt_accept_source_cancel(ntt_accept_source_t* self)
{
    if (shutdown(self->sock, SHUT_RD) == -1) {
        printf("shutdown failed: %s (%i)", strerror(errno), errno);
        abort();
    }
}

void ntt_accept_source_acquire(ntt_accept_source_t* self) { }

void ntt_accept_source_release(ntt_accept_source_t* self) { }
