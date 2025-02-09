#pragma once

#include "ntt/cstr.h"
#include "ntt/defs.h"
#include "ntt/export.h"

EXTERN_START

struct ntt_ec_category_tbl {
    ntt_cstr_t (*msg)(int ec);
    const char* (*name)();
};

typedef struct ntt_ec_category_tbl ntt_ec_category_tbl_t;

typedef struct ntt_ec_category {
    const ntt_ec_category_tbl_t* tbl;
} ntt_ec_category_t;

typedef struct ntt_ec ntt_ec_t;

struct ntt_ec {
    int ec;
    ntt_ec_category_t category;
};

static inline int ntt_ec_error(ntt_ec_t* self) { return self->ec; }
static inline ntt_cstr_t ntt_ec_msg(ntt_ec_t* self) { return self->category.tbl->msg(self->ec); }
static inline ntt_ec_category_t ntt_ec_category(ntt_ec_t* self) { return self->category; }

NTT_EXPORT extern const ntt_ec_category_tbl_t ntt_system_error_category_tbl;

static inline ntt_ec_t ntt_make_system_ec(int rc)
{
    ntt_ec_t ec;
    ec.category.tbl = &ntt_system_error_category_tbl;
    ec.ec = rc;
    return ec;
}

static inline ntt_ec_t ntt_make_ok_ec()
{
    ntt_ec_t ec;
    ec.category.tbl = NULL;
    ec.ec = 0;
    return ec;
}

EXTERN_STOP
