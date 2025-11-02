#pragma once

#include "ntt/char_span.h"
#include "ntt/defs.h"

EXTERN_START

static inline size_t ntt_char_format_to(char self, char* buf, size_t size)
{
    if (buf != NULL) {
        if (ntt_unlikely(size == 0)) {
            return 0;
        }
        buf[0] = self;
    }
    return 1;
}

EXTERN_STOP
