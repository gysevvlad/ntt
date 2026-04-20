#pragma once

#include "./queue.h"
#include "ntt/defs.h"
#include "ntt/impl/atomic.h"
#include "ntt/task.h"

EXTERN_START

#include <pthread.h>
#include <signal.h>

typedef struct ntt_worker ntt_worker_t;
typedef struct ntt_worker_cbs ntt_worker_cbs_t;

struct ntt_worker_cbs {
    void (*enter_cb)(void* ctx, ntt_worker_t* worker);
    void (*svc_cb)(void* ctx, sigset_t* sigset);
    void (*leave_cb)(void* cxt);
};

struct ntt_worker {
    // work quard count
    atomic_size_t refs;

    ntt_worker_cbs_t cbs;
    void* ctx;

    pthread_t id;
    size_t key;

    ntt_queue_t local_queue;
    sig_atomic_t need_drain_local_queue;

    // ntt_worker_task_queue_t task_queue;

    ntt_task_node_t stop_task_node;
    int stopped;
};

ntt_worker_t* ntt_worker_acquire(
    ntt_worker_t* self);

void ntt_worker_release(
    ntt_worker_t* self);

int ntt_worker_svc(
    ntt_worker_cbs_t cbs,
    void* ctx);

int ntt_worker_svc_with_mask(
    ntt_worker_cbs_t cbs,
    sigset_t* origin_mask,
    void* ctx);

ntt_worker_t* ntt_worker_self();

/**
 * @brief Push task to the local queue of the worker.
 */
void ntt_worker_push_task(
    ntt_worker_t* self,
    ntt_task_t* task);

void ntt_worker_init(
    ntt_worker_t* self,
    ntt_worker_cbs_t cbs,
    void* ctx);

void ntt_worker_deinit(
    ntt_worker_t* self);

void ntt_worker_wakeup_impl(
    ntt_worker_t* self);

size_t ntt_this_worker_key();

ntt_worker_t* ntt_worker_self();

EXTERN_STOP
