#pragma once

#include <stddef.h>

typedef struct ntt_char_span {
    size_t len;
    char* ptr;
} ntt_char_span_t;

static inline ntt_char_span_t ntt_char_span_from_len_and_ptr(size_t len,
    char* ptr)
{
    ntt_char_span_t span = { .len = len, .ptr = ptr };
    return span;
}

static inline ntt_char_span_t ntt_char_span_append_char(ntt_char_span_t span,
    char value)
{
    if (span.len == 0) {
        return span;
    }
    span.ptr[0] = value;
    --span.len;
    return span;
}
