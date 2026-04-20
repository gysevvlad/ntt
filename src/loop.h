#pragma once

#include "./queue.h"
#include "combine.h"
#include "ntt/defs.h"
#include "ntt/impl/atomic.h"
#include "ntt/impl/epoll_event.h"
#include "ntt/queue.h"
#include <ntt/loop.h>

#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>

EXTERN_START

typedef struct ntt_worker ntt_worker_t;

typedef struct ntt_loop_worker_ctx {
    pthread_t thread_id;
    ntt_worker_t* worker;
    ntt_loop_t* loop;
} ntt_loop_worker_ctx_t;

struct ntt_loop {
    ntt_loop_cbs_t cbs;
    void* ctx;

    int epoll_fd;

    atomic_size_t work_cnt;

    // int followers_created;
    // unsigned followers_ready;
    // volatile sig_atomic_t got_sigint;
    // volatile sig_atomic_t got_sighup;
    // volatile sig_atomic_t got_sigterm;

    pthread_mutex_t mtx;
    pthread_cond_t cv;
    unsigned enter_cnt;
    unsigned width;
    ntt_combine_t combine;

    // pthread_cond_t cnd;
    ntt_loop_worker_ctx_t workers[];
};

void ntt_loop_destroy(ntt_loop_t* self);

void ntt_loop_work_enter(ntt_loop_t* self);

void ntt_loop_work_leave(ntt_loop_t* self);

// int ntt_loop_add_epoll_event(ntt_loop_t* self, ntt_epoll_event_t* epoll_event);

// int ntt_loop_del_epoll_event(ntt_loop_t* self, ntt_epoll_event_t* epoll_event);

ntt_loop_t* ntt_loop_self();

EXTERN_STOP