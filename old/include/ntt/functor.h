#pragma once

#include "ntt/callback.h"
#include "ntt/defs.h"

#include <stddef.h>

EXTERN_START

struct ntt_functor {
  ntt_callback *callback;
  void *context;
};

#define ntt_functor_call_and_forget(functor)                                   \
  (functor)->callback((functor)->context);                                     \
  (functor)->callback = NULL;                                                  \
  (functor)->context = NULL

#define ntt_functor_init(functor)                                              \
  (functor)->context = NULL;                                                   \
  (functor)->callback = NULL;

inline void ntt_functor_setup(struct ntt_functor *functor,
                              ntt_callback *callback, void *context) {
  functor->callback = callback;
  functor->context = context;
}

EXTERN_STOP
