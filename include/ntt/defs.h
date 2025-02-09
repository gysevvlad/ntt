#pragma once

#include "ntt/export.h"

#ifdef __cplusplus
#define EXTERN_START extern "C" {
#define EXTERN_STOP }
#else
#define EXTERN_START
#define EXTERN_STOP
#endif

#define ntt_container_of(ptr, type, member) \
    ((type*)((char*)(ptr)-offsetof(type, member)))

#define ntt_likely(x) (__builtin_expect(!!(x), 1))
#define ntt_unlikely(x) (__builtin_expect(!!(x), 0))
