#pragma once

#include <ntt/defs.h>

#include <stdint.h>

EXTERN_START

typedef struct ntt_deadline_timer ntt_deadline_timer_t;

typedef uint64_t ntt_millis;

ntt_deadline_timer_t* ntt_deadline_timer_create();

void ntt_deadline_timer_reschedule(
    ntt_deadline_timer_t* self,
    ntt_millis timeout);

ntt_deadline_timer_t* ntt_deadline_timer_cancel();

EXTERN_STOP
