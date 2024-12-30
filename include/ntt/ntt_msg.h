#pragma once

#include "ntt/defs.h"
#include "ntt/impl/atomic.h"
#include "ntt/impl/node.h"
#include "ntt/ntt_buffer.h"

EXTERN_START

typedef struct ntt_msg ntt_msg_t;

NTT_EXPORT ntt_msg_t* ntt_msg_create(size_t capacity);

NTT_EXPORT const uint8_t* ntt_msg_cdata(ntt_msg_t* self);

NTT_EXPORT uint8_t* ntt_msg_data(ntt_msg_t* self);

EXTERN_STOP
