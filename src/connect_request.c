#include "ntt/impl/connect_request.h"
#include "ntt/connect_request.h"
#include "ntt/impl/pool.h"
#include "ntt/impl/socket.h"
#include "ntt/pool.h"
#include "ntt/sockaddr.h"

#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

void ntt_connect_request_svc(void* ctx, uint32_t events)
{
    (void)events;
    ntt_connect_request_t* self = ctx;

    // https://cr.yp.to/docs/connect.html
    struct sockaddr_storage ss;
    socklen_t len;
    int rc = getpeername(self->sock, (struct sockaddr*)&ss, &len);
    if (rc != 0) {
        char ch;
        rc = read(self->sock, &ch, 1);
        assert(rc == -1);
        int ec = errno;
        self->connect_cb(self->connect_ctx, ec, -1, NULL, self->addr);
        return;
    }

    ntt_sockaddr_t* local_addr = ntt_sockaddr_create_for_overriding();
    socklen_t local_addr_len;
    rc = getsockname(self->sock, (struct sockaddr*)&local_addr->storage, &local_addr_len);

    self->connect_cb(self->connect_ctx, 0, self->sock, local_addr, self->addr);
}

ntt_connect_request_t* ntt_connect_request_create()
{
    return malloc(sizeof(ntt_connect_request_t));
}

void ntt_connect_request_do(
    ntt_connect_request_t* self,
    ntt_pool_t* pool,
    ntt_sockaddr_t* addr,
    ntt_connect_cb_t* connect_cb,
    void* connect_ctx)
{
    self->pool = ntt_pool_acquire(pool);
    self->event.cb = ntt_connect_request_svc;
    self->event.ctx = self;
    self->sock = -1;
    self->addr = ntt_sockaddr_acquire(addr);
    self->connect_cb = connect_cb;
    self->connect_ctx = connect_ctx;

    int ec = ntt_socket_make_from_sockaddr(&self->sock, addr);

    if (ec != 0) {
        self->connect_cb(self->connect_ctx, ec, -1, NULL, self->addr);
        ntt_sockaddr_release(self->addr);
        return;
    }

    ec = ntt_socket_connect(&self->sock, addr);

    if (ec == 0) {
        ntt_sockaddr_t* local_addr = ntt_sockaddr_create_for_overriding();
        socklen_t local_addr_len;
        int rc = getsockname(self->sock, (struct sockaddr*)&local_addr->storage, &local_addr_len);

        if (rc != 0) {
            ec = errno;
            ntt_socket_close(&self->sock);
            ntt_sockaddr_release(local_addr);
            self->connect_cb(self->connect_ctx, ec, -1, NULL, addr);
            return;
        }

        self->connect_cb(self->connect_ctx, ec, self->sock, local_addr, addr);
        ntt_sockaddr_release(local_addr);
        return;
    }

    if (ec == EAGAIN || ec == EINPROGRESS) {
        struct epoll_event event;
        event.data.ptr = &self->event;
        event.events = EPOLLOUT | EPOLLONESHOT;

        int rc = epoll_ctl(self->pool->epoll_fd, EPOLL_CTL_ADD, self->sock, &event);

        if (rc == -1) {
            ec = errno;
            ntt_socket_close(&self->sock);
            self->connect_cb(self->connect_ctx, ec, -1, NULL, self->addr);
            ntt_sockaddr_release(self->addr);
            self->addr = NULL;
            return;
        }
    }
}

void ntt_connect_request_delete(
    ntt_connect_request_t* self)
{
    free(self);
}
