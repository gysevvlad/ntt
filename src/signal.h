#pragma once

#include "ntt/defs.h"

EXTERN_START

#include <signal.h>

/**
 * @brief Internal ntt signal for worker notification.
 */
#define NTT_SIGRTUP SIGRTMIN

/**
 * @brief Check that sigup signal blocked.
 */
int ntt_sigup_blocked();

void ntt_print_blocked_signals(sigset_t* sigset);

EXTERN_STOP
