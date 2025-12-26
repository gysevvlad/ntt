#pragma once

#include "ntt/impl/task_list.h"

#include <pthread.h>
#include <signal.h>

typedef struct ntt_worker ntt_worker_t;

typedef struct ntt_worker_task_queue {
    ntt_worker_t* worker;
    volatile sig_atomic_t up;
    pthread_spinlock_t lock;
    ntt_task_list_t tasks;
    int requested_flag;
    int wakeup_task_requested_flag;
    int wakeup_task_done_flag;
    ntt_task_node_t wakeup_task_node;
} ntt_worker_task_queue_t;

void ntt_worker_task_queue_init_impl(
    ntt_worker_task_queue_t* self,
    ntt_worker_t* worker);

void ntt_worker_task_queue_deinit_impl(
    ntt_worker_task_queue_t* self);

void ntt_worker_task_queue_svc_impl(
    ntt_worker_task_queue_t* self);

void ntt_worker_task_queue_send_task_impl(
    ntt_worker_task_queue_t* self,
    ntt_task_t* task);

void ntt_worker_task_queue_idle_post_impl(
    ntt_worker_task_queue_t* self,
    ntt_worker_task_queue_t* sender_queue,
    ntt_task_t* task);

void ntt_worker_task_queue_post_impl(
    ntt_worker_task_queue_t* self,
    ntt_task_t* task);

void ntt_worker_task_queue_defer_wakeup_impl(
    ntt_worker_task_queue_t* self,
    ntt_worker_task_queue_t* sender);
