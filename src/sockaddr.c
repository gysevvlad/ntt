#include "ntt/sockaddr.h"

#include "ntt/char_span.h"
#include "ntt/impl/atomic.h"
#include "ntt/impl/sockaddr.h"
#include "ntt/impl/sockaddr_in.h"
#include "ntt/impl/sockaddr_in6.h"
#include "ntt/impl/sockaddr_un.h"
#include "ntt/view.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ntt_sockaddr_from_view(ntt_sockaddr_t* self, ntt_view_t view)
{
    if (ntt_sockaddr_in_from_view((struct sockaddr_in*)&self->storage, view)) {
        return 1;
    }
    if (ntt_sockaddr_in6_from_view((struct sockaddr_in6*)&self->storage, view)) {
        return 1;
    }
    if (ntt_sockaddr_un_from_view((struct sockaddr_un*)&self->storage, view)) {
        return 1;
    }
    return 0;
}

ntt_sockaddr_t* ntt_sockaddr_make_from_view(ntt_view_t view)
{
    ntt_sockaddr_t* self = malloc(sizeof(ntt_sockaddr_t));
    if (!ntt_sockaddr_from_view(self, view)) {
        free(self);
        return NULL;
    }
    self->refs = 1;
    return self;
}

size_t ntt_sockaddr_formatted_size(const ntt_sockaddr_t* self)
{
    switch (self->storage.ss_family) {
    case AF_INET:
        return ntt_sockaddr_in_formatted_size((struct sockaddr_in*)&self->storage);
    case AF_INET6:
        return ntt_sockaddr_in6_formatted_size(
            (struct sockaddr_in6*)&self->storage);
    case AF_UNIX:
        return ntt_sockaddr_un_formatted_size((struct sockaddr_un*)&self->storage);
    }
    return 0;
}

ntt_char_span_t ntt_sockaddr_format_to(const ntt_sockaddr_t* self,
    ntt_char_span_t buffer)
{
    switch (self->storage.ss_family) {
    case AF_INET:
        return ntt_sockaddr_in_format_to((struct sockaddr_in*)&self->storage,
            buffer);
    case AF_INET6:
        return ntt_sockaddr_in6_format_to((struct sockaddr_in6*)&self->storage,
            buffer);
    case AF_UNIX:
        return ntt_sockaddr_un_format_to((struct sockaddr_un*)&self->storage,
            buffer);
    }
    return buffer;
}

ntt_sockaddr_t* ntt_sockaddr_acquire(ntt_sockaddr_t* self)
{
    atomic_fetch_add(&self->refs, 1);
    return self;
}

void ntt_sockaddr_release(ntt_sockaddr_t* self)
{
    if (atomic_fetch_sub(&self->refs, 1)) {
        free(self);
    }
}

int ntt_create_and_bind_socket(ntt_sockaddr_t* addr)
{
    int sock_fd = socket(addr->storage.ss_family, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        return 0;
    }
    int len = 0;
    switch (addr->storage.ss_family) {
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
    if (bind(sock_fd, (struct sockaddr*)&addr->storage, len) != 0) {
        close(sock_fd);
        return 0;
    }
    close(sock_fd);
    return 1;
}
