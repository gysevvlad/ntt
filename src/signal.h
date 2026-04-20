#pragma once

#include "ntt/defs.h"

EXTERN_START

#include <signal.h>

/**
 * @brief Internal ntt signal for worker notification.
 */
#define NTT_SIGRTUP SIGRTMIN

EXTERN_STOP
