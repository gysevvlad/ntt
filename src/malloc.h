#pragma once

#include "ntt/defs.h"

EXTERN_START

#include <stddef.h>

void* ntt_malloc(size_t s);

void ntt_free(void* ptr);

EXTERN_STOP
