#include "ntt/accept_source.h"

#include "ntt/ec.h"
#include "ntt/impl/epoll_event.h"
#include "ntt/impl/loop.h"
#include "ntt/impl/malloc.h"
#include "ntt/impl/socket.h"
#include "ntt/sockaddr.h"

struct ntt_accept_source {
    const ntt_accept_source_vtbl_t* listener;
    void* ctx;
    ntt_sockaddr_t* sockaddr;
    ntt_socket_t sock;
    ntt_ec_t ec;
    struct ntt_epoll_event epoll_event;
};

void ntt_accept_source_construct(
    ntt_accept_source_t* self,
    const ntt_accept_source_vtbl_t* listener,
    void* ctx,
    ntt_sockaddr_t* sockaddr)
{
    self->listener = listener;
    self->ctx = ctx;
    self->sockaddr = sockaddr;
    self->sock = NTT_INVALID_SOCKET;
    self->ec = ntt_make_system_ec(0);
}

ntt_accept_source_t* ntt_accept_source_create(
    const ntt_accept_source_vtbl_t* listener,
    void* ctx,
    ntt_sockaddr_t* sockaddr)
{
    ntt_accept_source_t* self = ntt_malloc(sizeof(ntt_accept_source_t));

    if (self == NULL) {
        return NULL;
    }

    ntt_accept_source_construct(self, listener, ctx, sockaddr);

    return self;
}

void ntt_accept_source_start(
    ntt_accept_source_t* self,
    ntt_loop_t* loop)
{
    int rc = 0;

    ntt_ec_t ec;

    rc = ntt_socket_make_from_sockaddr(
        &self->sock,
        self->sockaddr);

    if (rc != 0) {
        self->ec = ntt_make_system_ec(rc);
        // TODO(vgusev): push work to loop
        return;
    }

    rc = ntt_socket_set_reuse_addr(
        &self->sock);

    if (rc != 0) {
        self->ec = ntt_make_system_ec(rc);
        // TODO(vgusev): push work to loop
        return;
    }

    rc = ntt_socket_bind(
        &self->sock,
        self->sockaddr);

    if (rc != 0) {
        ec = ntt_make_system_ec(rc);
        // TODO(vgusev): push work to loop
        return;
    }

    rc = ntt_socket_listen(&self->sock);

    if (rc != 0) {
        ec = ntt_make_system_ec(rc);
        // TODO(vgusev): push work to loop
        return;
    }

    ntt_loop_add_epoll_event(
        loop,
        &self->epoll_event);
}

void ntt_accept_source_stop(
    ntt_accept_source_t* self)
{
}

void ntt_accept_source_destroy(
    ntt_accept_source_t* self)
{
}
