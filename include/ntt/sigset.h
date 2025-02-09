#pragma once

#include "ntt/defs.h"

#include <stdint.h>

EXTERN_START

#define NTT_SIGSET_SIZE 128

typedef struct ntt_sigset {
    uint8_t data[NTT_SIGSET_SIZE];
} ntt_sigset_t;

#undef NTT_SIGSET_SIZE

EXTERN_STOP
