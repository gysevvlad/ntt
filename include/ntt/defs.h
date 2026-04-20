#pragma once

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

#ifdef NTT_STATIC_DEFINE
#  define NTT_EXPORT
#  define NTT_NO_EXPORT
#else
#  ifndef NTT_EXPORT
#    ifdef ntt_EXPORTS
        /* We are building this library */
#      define NTT_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define NTT_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef NTT_NO_EXPORT
#    define NTT_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef NTT_DEPRECATED
#  define NTT_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef NTT_DEPRECATED_EXPORT
#  define NTT_DEPRECATED_EXPORT NTT_EXPORT NTT_DEPRECATED
#endif

#ifndef NTT_DEPRECATED_NO_EXPORT
#  define NTT_DEPRECATED_NO_EXPORT NTT_NO_EXPORT NTT_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef NTT_NO_DEPRECATED
#    define NTT_NO_DEPRECATED
#  endif
#endif
