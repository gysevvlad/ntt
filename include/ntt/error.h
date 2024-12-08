#pragma once

#include "ntt/defs.h"

EXTERN_START

struct ntt_error_category {
};

typedef struct ntt_error_category ntt_error_category_t;

struct ntt_error {
    int error;
    ntt_error_category_t* category;
};

EXTERN_STOP
