#pragma once

#include <ntt/defs.h>
#include <ntt/ec.h>
#include <ntt/loop.h>

#include <stdint.h>
#include <time.h>

EXTERN_START

typedef struct ntt_timer ntt_timer_t;

typedef uint64_t ntt_millis;

int ntt_timer_create(
    ntt_timer_t** self);

ntt_ec_t ntt_timer_start(
    ntt_timer_t* self,
    ntt_loop_t* loop,
    struct timespec delay);

void ntt_timer_cancel(
    ntt_timer_t* self);

NTT_EXPORT void ntt_reader_delete(
    ntt_timer_t* self);

// void ntt_deadline_timer_reschedule(
//     ntt_deadline_timer_t* self,
//     ntt_millis timeout);

// ntt_deadline_timer_t* ntt_deadline_timer_cancel();

EXTERN_STOP
