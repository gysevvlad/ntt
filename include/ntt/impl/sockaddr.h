#pragma once

#include "ntt/defs.h"
#include "ntt/impl/atomic.h"
#include "ntt/sockaddr.h"

#include <sys/socket.h>

#include <stdlib.h>

EXTERN_START

struct ntt_sockaddr {
    atomic_size_t refs;
    struct sockaddr_storage storage;
};

static inline ntt_sockaddr_t* ntt_sockaddr_create_for_overriding()
{
    ntt_sockaddr_t* self = (ntt_sockaddr_t*)malloc(sizeof(ntt_sockaddr_t));
    self->refs = 1;
    return self;
}

EXTERN_STOP
