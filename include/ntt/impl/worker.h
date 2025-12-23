#pragma once

#include "ntt/defs.h"
#include "ntt/worker.h"

#include "ntt/impl/atomic.h"
#include "ntt/impl/task_list.h"

#include <pthread.h>
#include <signal.h>

EXTERN_START

struct ntt_worker {
    atomic_size_t refs;

    ntt_worker_cbs_t cbs;
    void* ctx;

    pthread_t id;

    volatile sig_atomic_t tasks_up;
    pthread_spinlock_t tasks_lock;
    ntt_task_list_t tasks_lists;

    ntt_task_node_t stop_task_node;

    int stopped;
};

void ntt_worker_construct(
    ntt_worker_t* self,
    ntt_worker_cbs_t cbs,
    void* ctx);

void ntt_worker_destruct(
    ntt_worker_t* self);

EXTERN_STOP
