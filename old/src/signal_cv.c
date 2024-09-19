#include "ntt/signal_cv.h"
#include "ntt/callback.h"
#include "ntt/debug.h"
#include "ntt/functor.h"

int ntt_signal_cv_init(struct ntt_signal_cv *signal) {
  int ec;

  ec = pthread_cond_init(&signal->cv, NULL);
  DEBUG_ASSERT(ec == 0);
  if (ec != 0) {
    return ec;
  }

  ec = pthread_mutex_init(&signal->m, NULL);
  DEBUG_ASSERT(ec == 0);
  if (ec != 0) {
    pthread_cond_destroy(&signal->cv);
    return ec;
  }

  signal->cnt = 0;
  signal->stopped = 0;
  ntt_functor_init(&signal->stop_functor);
  signal->canceled = 0;

  DEBUG_ACTION({ signal->initialized = 1; });

  return 0;
}

int ntt_signal_cv_wait(struct ntt_signal_cv *signal) {
  DEBUG_ASSERT(signal->initialized == 1);

  int ec;

  int stopped = 0;

  ec = pthread_mutex_lock(&signal->m);
  DEBUG_ASSERT(ec == 0);

  DEBUG_ASSERT(signal->cnt >= 0);

  while (signal->cnt == 0 && signal->canceled == 0) {
    ec = pthread_cond_wait(&signal->cv, &signal->m);
    DEBUG_ASSERT(ec == 0);
  }

  if (signal->cnt > 0) {
    signal->cnt -= 1;
  } else if (signal->stopped == 1) {
    stopped = 1;
  } else if (signal->canceled == 1) {
    ntt_functor_call_and_forget(&signal->stop_functor);
    signal->stopped = 1;
    stopped = 1;
  } else {
    DEBUG_ASSERT(1 == 0);
  }

  ec = pthread_mutex_unlock(&signal->m);
  DEBUG_ASSERT(ec == 0);

  return stopped == 0 ? 0 : -1;
}

void ntt_signal_cv_notify(struct ntt_signal_cv *signal) {
  DEBUG_ASSERT(signal->initialized == 1);

  int ec;

  ec = pthread_mutex_lock(&signal->m);
  DEBUG_ASSERT(ec == 0);

  DEBUG_ASSERT(signal->stopped == 0);

  signal->cnt += 1;

  ec = pthread_cond_signal(&signal->cv);
  DEBUG_ASSERT(ec == 0);

  ec = pthread_mutex_unlock(&signal->m);
  DEBUG_ASSERT(ec == 0);
}

void ntt_signal_cv_cancel(struct ntt_signal_cv *signal, void *context,
                          ntt_callback *callback) {
  DEBUG_ASSERT(signal->initialized == 1);

  int ec;

  ec = pthread_mutex_lock(&signal->m);
  DEBUG_ASSERT(ec == 0);

  DEBUG_ASSERT(signal->stopped == 0);

  if (signal->canceled == 0) {
    signal->stop_functor.callback = callback;
    signal->stop_functor.context = context;
    signal->canceled = 1;
    ec = pthread_cond_broadcast(&signal->cv);
    DEBUG_ASSERT(ec == 0);
  }

  ec = pthread_mutex_unlock(&signal->m);
  DEBUG_ASSERT(ec == 0);
}

void ntt_signal_cv_destroy(struct ntt_signal_cv *signal) {
  DEBUG_ASSERT(signal->initialized == 1);

  int ec;

  ec = pthread_cond_destroy(&signal->cv);
  DEBUG_ASSERT(ec == 0);

  ec = pthread_mutex_destroy(&signal->m);
  DEBUG_ASSERT(ec == 0);

  DEBUG_ACTION({ signal->initialized = 0; });
  DEBUG_ASSERT(signal->cnt == 0);
  DEBUG_ASSERT(signal->canceled == 0);
  DEBUG_ASSERT(signal->stopped == 1);
}
