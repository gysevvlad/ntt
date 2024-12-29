#pragma once

#include "ntt/pool.h"

#include "ntt/impl/atomic.h"
#include "ntt/impl/ntt_task_cache.h"
#include "ntt/impl/task_list.h"
#include "ntt/impl/thread.h"

struct ntt_pool {
    atomic_size_t internal_refs;
    atomic_size_t external_refs;

    size_t thread_cnt;
    int epoll_fd;
    int eventfd;

    ntt_task_list_t tasks;
    int task_awaiters;
    pthread_spinlock_t tasks_lock;

    ntt_task_cache_t* task_cache;

    // stopped callback
    ntt_pool_stopped_cb* stopped_cb;
    void* stopped_ctx;

    ntt_thread_t threads[];
};

void ntt_pool_send_task_to_all(ntt_pool_t* self, ntt_task_t* task);
