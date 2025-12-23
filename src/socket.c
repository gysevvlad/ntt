#include "ntt/socket.h"

#include "./socket_impl.h"
#include "ntt/defs.h"
#include "ntt/impl/malloc.h"

#include <sys/socket.h>
#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>

ntt_socket_t* ntt_socket_crete()
{
    ntt_socket_t* self = ntt_malloc(sizeof(ntt_socket_t));
    if (ntt_unlikely(self == NULL)) {
        return NULL;
    }
    self->fd = NTT_INVALID_SOCKET;
    return self;
}

int ntt_socket_open(ntt_socket_t* self, const ntt_sockaddr_t* sockaddr)
{
    self->fd = socket(
        sockaddr->storage.ss_family,
        SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
        0);

    if (self->fd == -1) {
        return errno;
    }

    return 0;
}

ntt_socket_t* ntt_socket_init_from_ipv4_and_port(
    ntt_sockaddr_t* self,
    const char* ipv4,
    uint16_t port)
{
    return NULL;
}

void ntt_socket_delete(ntt_socket_t* self)
{
    assert(self != NULL);
    if (self->fd != NTT_INVALID_SOCKET) {
        close(self->fd);
    }
    ntt_free(self);
}

void ntt_socket_connect(ntt_socket_t* self, ntt_loop_t* loop)
{
}
