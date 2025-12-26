#pragma once

#include "ntt/defs.h"
#include "ntt/worker.h"

#include "ntt/impl/atomic.h"
#include "ntt/impl/task_list.h"
#include "ntt/impl/worker_task_queue.h"

#include <pthread.h>
#include <signal.h>

EXTERN_START

struct ntt_worker {
    atomic_size_t refs;

    ntt_worker_cbs_t cbs;
    void* ctx;

    pthread_t id;
    size_t key;

    ntt_worker_task_queue_t task_queue;

    ntt_task_node_t stop_task_node;
    int stopped;
};

void ntt_worker_init(
    ntt_worker_t* self,
    ntt_worker_cbs_t cbs,
    void* ctx);

void ntt_worker_push_task_impl(
    ntt_worker_t* self,
    ntt_task_t* task);

void ntt_worker_deinit(
    ntt_worker_t* self);

void ntt_worker_wakeup_impl(
    ntt_worker_t* self);

size_t ntt_this_thread_key();

size_t ntt_this_worker_key();

EXTERN_STOP
