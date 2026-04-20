#include "./malloc.h"

#include <stdlib.h>

void* ntt_malloc(size_t s)
{
    return malloc(s);
}

void ntt_free(void* ptr)
{
    return free(ptr);
}
