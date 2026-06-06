#pragma once

#include "ntt/defs.h"

EXTERN_START

#include <stddef.h>

void* ntt_malloc(size_t s) __attribute__((returns_nonnull));

void* ntt_malloc_or_die(size_t s) __attribute__((returns_nonnull));

void ntt_free(void* ptr);

EXTERN_STOP
