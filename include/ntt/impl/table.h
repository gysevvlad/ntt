#pragma once

#include "ntt/defs.h"
#include "ntt/impl/atomic.h"

#include <stdint.h>

EXTERN_START

struct ntt_table_slot {
    atomic_uint_fast64_t state;
};

typedef struct ntt_table_slot ntt_table_slot_t;

#define NTT_TABLE_SLOT_CNT 1024 * 64

struct ntt_table {
    ntt_table_slot_t chunks[NTT_TABLE_SLOT_CNT];
};

typedef struct ntt_table ntt_table_t;

ntt_table_t* ntt_table_create();

void ntt_table_destroy(ntt_table_t* table);

EXTERN_STOP
