#pragma once

#include "ntt/debug.h"
#include "ntt/defs.h"
#include "ntt/functor.h"

#include <pthread.h>

EXTERN_START

struct ntt_signal_cv;

struct ntt_signal_cv {
  pthread_cond_t cv;
  pthread_mutex_t m;
  int cnt;
  int stopped;
  int canceled;
  struct ntt_functor stop_functor;

  DEBUG_FIELD(int, initialized);
};

int ntt_signal_cv_init(struct ntt_signal_cv *signal);

int ntt_signal_cv_wait(struct ntt_signal_cv *signal);

void ntt_signal_cv_notify(struct ntt_signal_cv *signal);

void ntt_signal_cv_cancel(struct ntt_signal_cv *signal, void *context,
                          ntt_callback *callback);

void ntt_signal_cv_destroy(struct ntt_signal_cv *signal);

EXTERN_STOP
