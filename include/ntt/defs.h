#pragma once

#ifdef __cplusplus
#define EXTERN_START extern "C" {
#define EXTERN_STOP }
#else
#define EXTERN_START
#define EXTERN_STOP
#endif

#define ntt_container_of(ptr, type, member)                                    \
  ((type *)((char *)(ptr)-offsetof(type, member)))
