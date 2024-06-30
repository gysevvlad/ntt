#include "ntt/signal.h"
#include "ntt/callback.h"
#include "ntt/signal_cv.h"

int ntt_signal_init(struct ntt_signal *signal, enum ntt_signal_type type) {
  signal->type = type;
  switch (type) {
  case NTT_SIGNAL_CV:
    return ntt_signal_cv_init(&signal->cv_signal);
  }
}

void ntt_signal_notify(struct ntt_signal *signal) {
  switch (signal->type) {
  case NTT_SIGNAL_CV:
    return ntt_signal_cv_notify(&signal->cv_signal);
  }
}

int ntt_signal_wait(struct ntt_signal *signal) {
  switch (signal->type) {
  case NTT_SIGNAL_CV:
    return ntt_signal_cv_wait(&signal->cv_signal);
  }
}

void ntt_signal_cancel(struct ntt_signal *signal, void *context,
                       ntt_callback *callback) {
  switch (signal->type) {
  case NTT_SIGNAL_CV:
    return ntt_signal_cv_cancel(&signal->cv_signal, context, callback);
  }
}
