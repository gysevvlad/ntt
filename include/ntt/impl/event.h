#pragma once

#include <stdint.h>

typedef struct ntt_event {
    void (*cb)(void* ctx, uint32_t events);
    void* ctx;
} ntt_event_t;
