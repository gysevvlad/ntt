#pragma once

#include "ntt/defs.h"

#include <stddef.h>

EXTERN_START

void* ntt_malloc(size_t s);

void ntt_free(void* ptr);

EXTERN_STOP
