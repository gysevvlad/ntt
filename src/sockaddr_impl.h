#pragma once

#include "ntt/sockaddr.h"

#include <sys/socket.h>

#include <stdint.h>

struct ntt_sockaddr {
    struct sockaddr_storage storage;
};

int ntt_sockaddr_init_from_ipv4_and_port(
    ntt_sockaddr_t* self,
    const char* ipv4,
    uint16_t port);
