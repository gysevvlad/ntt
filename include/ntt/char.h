#pragma once

#include "ntt/char_span.h"
#include "ntt/defs.h"

EXTERN_START

static inline ntt_char_span_t ntt_char_format_to(char self,
    ntt_char_span_t buffer)
{
    if (buffer.len > 0) {
        buffer.ptr[0] = self;
        buffer.len -= 1;
        buffer.ptr += 1;
    }
    return buffer;
}

EXTERN_STOP
