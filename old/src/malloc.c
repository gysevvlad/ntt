#include "ntt/malloc.h"

#include <stdlib.h>

void *ntt_malloc(size_t size) { return malloc(size); }

void ntt_free(void *ptr) { return free(ptr); }

void *ntt_calloc(size_t nmemb, size_t size) { return calloc(nmemb, size); }
