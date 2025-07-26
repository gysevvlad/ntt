#pragma once

#include <ntt/defs.h>

EXTERN_START

NTT_EXPORT void ntt_signal_setup(int sig);
NTT_EXPORT void ntt_signal_revert(int sig);
NTT_EXPORT int ntt_signal_get_eventfd(int sig);

EXTERN_STOP
