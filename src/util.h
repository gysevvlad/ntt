#pragma once

#include <stddef.h>
#include <stdint.h>

static inline int ntt_is_aligned(void *ptr, size_t align) {
  return !((uintptr_t)(ptr) % align);
}
