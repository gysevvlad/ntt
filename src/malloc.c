#include "./malloc.h"

#include "./die.h"

#include <assert.h>
#include <stdlib.h>

void* ntt_malloc(size_t s)
{
    void* ptr = malloc(s);
    assert(ptr != NULL);
    return ptr;
}

void* ntt_malloc_or_die(size_t s)
{
    void* ptr = malloc(s);
    if (ptr == NULL) {
        ntt_die();
    }
    return ptr;
}

void* ntt_realloc_or_die(void* ptr, size_t s)
{
    void* new_ptr = realloc(ptr, s);
    if (new_ptr == NULL) {
        ntt_die();
    }
    return new_ptr;
}

void ntt_free(void* ptr)
{
    return free(ptr);
}
