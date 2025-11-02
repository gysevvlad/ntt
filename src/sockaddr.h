#pragma once

#include <sys/socket.h>

struct ntt_sockaddr {
    struct sockaddr_storage storage;
};
