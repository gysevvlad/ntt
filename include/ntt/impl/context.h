#pragma once

#include "ntt/context.h"
#include "ntt/impl/atomic.h"
#include "ntt/impl/thread.h"

#include <pthread.h>

EXTERN_START

struct ntt_context {
    pthread_mutex_t mtx;
    ntt_thread_t** threads;
    size_t threads_cnt;
    int epoll_fd;
    atomic_size_t work_cnt;
    pthread_t leader_thread;
    int started;
};

EXTERN_STOP
