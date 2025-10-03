#pragma once

#include "ntt/defs.h"

#include <stddef.h>

EXTERN_START

typedef struct ntt_view {
    size_t len;
    const char* str;
} ntt_view_t;

/**
 * @brief Split view by delimeter.
 *
 * @return 1 on success, 0 on fail
 */
int ntt_view_split_by_char(
    ntt_view_t view,
    char delimeter,
    ntt_view_t* head,
    ntt_view_t* tail);

ntt_view_t ntt_view_from_cstr(const char* cstr);

#define ntt_view_from_literal(literal)             \
    {                                              \
        .len = sizeof(literal) - 1, .str = literal \
    }

EXTERN_STOP
