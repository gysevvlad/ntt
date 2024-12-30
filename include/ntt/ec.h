#pragma once

#include "ntt/defs.h"

EXTERN_START

struct ntt_ec_category {
    
};

typedef struct ntt_ec_category ntt_ec_category_tbl_t;

struct ntt_ec {
    int ec;
    ntt_ec_category_tbl_t* category;
};

EXTERN_STOP
