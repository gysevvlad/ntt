#pragma once

#include "ntt/defs.h"
#include "ntt/export.h"
#include "ntt/task.h"

EXTERN_START

typedef struct ntt_thread ntt_thread_t;

NTT_EXPORT ntt_thread_t* ntt_thread_create();

NTT_EXPORT void ntt_thread_set_epollfd(
    ntt_thread_t* self,
    int epollfd);

// NTT_EXPORT void ntt_thread_set_listener_tbl(
//     ntt_thread_t* self,
//     const ntt_thread_listener_tbl_t* listener_tbl);

NTT_EXPORT void ntt_thread_start(ntt_thread_t* self);

NTT_EXPORT void ntt_thread(ntt_thread_t* self);

NTT_EXPORT void ntt_thread_post_task(
    ntt_thread_t* self,
    ntt_task_t* task);

NTT_EXPORT void ntt_thread_cancel(
    ntt_thread_t* self);

EXTERN_STOP
