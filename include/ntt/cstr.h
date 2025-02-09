#pragma once

#include "ntt/defs.h"
#include "ntt/export.h"
#include "ntt/view.h"

#include <stdlib.h>
#include <string.h>

EXTERN_START

typedef struct ntt_cstr {
    char* data;
} ntt_cstr_t;

static inline ntt_cstr_t ntt_cstr_dup(const char* str)
{
    ntt_cstr_t cstr;
    cstr.data = strdup(str);
    return cstr;
}

static inline void ntt_cstr_free(ntt_cstr_t* self)
{
    free(self);
}

NTT_EXPORT ntt_cstr_t ntt_cstr_from_view(ntt_view_t view);

EXTERN_STOP
