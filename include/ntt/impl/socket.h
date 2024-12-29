#pragma once

#include "ntt/defs.h"
#include "ntt/sockaddr.h"

#include "ntt/impl/sockaddr.h"

#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

typedef int ntt_socket_t;

static inline int ntt_socket_make_from_sockaddr(int* self, ntt_sockaddr_t* sockaddr)
{
    assert(*self == -1);

    int type = SOCK_CLOEXEC | SOCK_STREAM | SOCK_NONBLOCK;
    int sock = socket(sockaddr->storage.ss_family, type, 0);
    if (sock == -1) {
        return errno;
    }
    *self = sock;
    return 0;
}

static inline int ntt_socket_set_reuse_addr(int* self)
{
    assert(*self != -1);

    if (setsockopt(*self, SOL_SOCKET, SO_REUSEADDR, &(int) { 1 }, sizeof(int)) != 0) {
        return errno;
    }
    return 0;
}

static inline int ntt_socket_bind(int* self, ntt_sockaddr_t* sockaddr)
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
        return errno;
    }

    return 0;
}

static inline socklen_t ntt_sockaddr_len(ntt_sockaddr_t* addr)
{
    switch (addr->storage.ss_family) {
    case AF_INET:
        return sizeof(struct sockaddr_in);
    case AF_INET6:
        return sizeof(struct sockaddr_in6);
    case AF_UNIX:
        return sizeof(struct sockaddr_un);
    default:
        return 0;
    }
}

static inline int ntt_socket_connect(int* self, ntt_sockaddr_t* addr)
{
    int len = 0;

    if (connect(*self, (struct sockaddr*)&addr->storage, ntt_sockaddr_len(addr)) != 0) {
        return errno;
    }

    return 0;
}

static inline int ntt_socket_listen(int* self)
{
    if (listen(*self, SOMAXCONN) != 0) {
        return errno;
    }
    return 0;
}

static inline void ntt_socket_close(int* self)
{
    close(*self);
    *self = -1;
}
