#pragma once

#include "ntt/callback.h"
#include "ntt/defs.h"
#include "ntt/signal_cv.h"

EXTERN_START

enum ntt_signal_type {
  NTT_SIGNAL_CV,
};

struct ntt_signal {
  enum ntt_signal_type type;
  union {
    struct ntt_signal_cv cv_signal;
  };
};

int ntt_signal_init(struct ntt_signal *signal, enum ntt_signal_type type);

void ntt_signal_notify(struct ntt_signal *signal);

int ntt_signal_wait(struct ntt_signal *signal);

void ntt_signal_cancel(struct ntt_signal *signal, void *context,
                       ntt_callback *callback);

EXTERN_STOP
