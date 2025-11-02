#include "ntt/sockaddr.h"

#include "./sockaddr.h"
#include "ntt/impl/malloc.h"
#include "ntt/util.h"

#include "ntt/char_span.h"
#include "ntt/impl/atomic.h"
#include "ntt/view.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ntt_sockaddr_t* ntt_sockaddr_create_from_ipv4_and_port(const char* ipv4, uint16_t port)
{
    ntt_sockaddr_t* self = ntt_malloc(sizeof(ntt_sockaddr_t));

    if (ntt_unlikely(self == NULL)) {
        return NULL;
    }

    struct sockaddr_in* sockaddr = (struct sockaddr_in*)&self->storage;

    sockaddr->sin_family = AF_INET;
    sockaddr->sin_port   = htons(port);

    int rc = inet_pton(AF_INET, ipv4, &sockaddr->sin_addr);

    if (ntt_unlikely(rc != 1)) {
        ntt_free(self);
        return NULL;
    }

    return self;
}

ntt_sockaddr_t* ntt_sockaddr_create_from_ipv6_and_port(const char* ipv6, uint16_t port)
{
    ntt_sockaddr_t* self = ntt_malloc(sizeof(ntt_sockaddr_t));

    if (ntt_unlikely(self == NULL)) {
        return NULL;
    }

    struct sockaddr_in6* sockaddr = (struct sockaddr_in6*)&self->storage;

    sockaddr->sin6_family   = AF_INET6;
    sockaddr->sin6_port     = htons(port);
    sockaddr->sin6_flowinfo = 0;

    int rc = inet_pton(AF_INET6, ipv6, &sockaddr->sin6_addr);

    if (ntt_unlikely(rc != 1)) {
        ntt_free(self);
        return NULL;
    }

    sockaddr->sin6_scope_id = 0;

    return self;
}

void ntt_sockaddr_delete(ntt_sockaddr_t* self)
{
    ntt_free(self);
}

size_t ntt_in_addr_format_to(struct in_addr* addr, char* buf, size_t size)
{
    assert(addr != NULL);

    char temp[INET_ADDRSTRLEN];
    const char* dst = inet_ntop(AF_INET, addr, temp, INET_ADDRSTRLEN);
    assert(dst != NULL);

    size_t addr_len = strlen(dst);

    if (buf == NULL) {
        return addr_len;
    }

    size_t len = addr_len < size ? addr_len : size;

    size_t i = 0;
    for (; i < len; ++i) {
        *(buf++) = dst[i];
    }
    return len;
}

size_t ntt_sockaddr_in_format_to(struct sockaddr_in* self, char* buf, size_t len)
{
    size_t size = 0;
    if (buf == NULL) {
        size += ntt_in_addr_format_to(&self->sin_addr, NULL, 0);
        size += ntt_char_format_to(':', NULL, 0);
        size += ntt_unsigned_short_format_to(ntohs(self->sin_port), NULL, 0);
    } else {
        size += ntt_in_addr_format_to(&self->sin_addr, buf + size, len - size);
        size += ntt_char_format_to(':', buf + size, len - size);
        size += ntt_unsigned_short_format_to(ntohs(self->sin_port), buf + size, len - size);
    }
    return size;
}

size_t ntt_in6_addr_format_to(struct in6_addr* addr, char* buf, size_t len)
{
    assert(addr != NULL);

    char temp[INET6_ADDRSTRLEN];
    const char* dst = inet_ntop(AF_INET6, addr, temp, INET6_ADDRSTRLEN);
    assert(dst != NULL);

    size_t size = strlen(dst);
    if (size > len) {
        size = len;
    }

    if (buf != NULL) {
        size_t i;
        for (i = 0; i < len; ++i) {
            buf[i] = dst[i];
        }
    }

    return size;
}

static size_t ntt_sockaddr_in6_format_to(struct sockaddr_in6* self, char* buf, size_t len)
{
    size_t size = 0;
    if (buf == NULL) {
        size += ntt_char_format_to('[', NULL, 0);
        size += ntt_in6_addr_format_to(&self->sin6_addr, NULL, 0);
        size += ntt_char_format_to(']', NULL, 0);
        size += ntt_char_format_to(':', NULL, 0);
        size += ntt_unsigned_short_format_to(ntohs(self->sin6_port), NULL, 0);

    } else {
        size += ntt_char_format_to('[', buf + size, len - size);
        size += ntt_in6_addr_format_to(&self->sin6_addr, buf + size, len - size);
        size += ntt_char_format_to(']', buf + size, len - size);
        size += ntt_char_format_to(':', buf + size, len - size);
        size += ntt_unsigned_short_format_to(ntohs(self->sin6_port), NULL, 0);
    }
    return size;
}

static size_t ntt_sockaddr_un_format_to(struct sockaddr_un* self, char* buf, size_t len)
{
    size_t size = strlen(self->sun_path);
    if (size > len) {
        size = len;
    }
    if (buf != NULL) {
        memcpy(buf, self->sun_path, size);
    }
    return size;
}

size_t ntt_sockaddr_format_to(const ntt_sockaddr_t* self, char* buf, size_t len)
{
    switch (self->storage.ss_family) {
    case AF_INET:
        return ntt_sockaddr_in_format_to((struct sockaddr_in*)&self->storage, buf, len);
    case AF_INET6:
        return ntt_sockaddr_in6_format_to((struct sockaddr_in6*)&self->storage, buf, len);
    case AF_UNIX:
        return ntt_sockaddr_un_format_to((struct sockaddr_un*)&self->storage, buf, len);
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
