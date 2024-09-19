#pragma once

#include <stddef.h>

void *ntt_malloc(size_t size);

void ntt_free(void *);

void *ntt_calloc(size_t nmemb, size_t size);
