#pragma once

#include "ntt/defs.h"
#include "ntt/impl/atomic.h"
#include "ntt/impl/node.h"

EXTERN_START

struct ntt_msg {
    atomic_size_t refs;
    ntt_node_t node;
    size_t cap;
    size_t len;
    uint8_t data[];
};

EXTERN_STOP
