#pragma once

#include <sys/epoll.h>

typedef struct ntt_source {
    int desc;
    struct epoll_event event;
} ntt_source_t;
