#pragma once

#include "ntt/defs.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sys/uio.h>

EXTERN_START

typedef struct ntt_fixed_circular_buffer {
    size_t pos;
    size_t len;
    size_t cap;
    uint8_t* data;
} ntt_fixed_circular_buffer_t;

static inline int ntt_fixed_circular_buffer_init(
    ntt_fixed_circular_buffer_t* self,
    size_t cap)
{
    self->pos = 0;
    self->len = 0;
    self->cap = cap;
    self->data = malloc(self->cap);

    if (ntt_unlikely(self->data == NULL)) {
        return ENOMEM;
    }

    return 0;
}

static inline int ntt_fixed_circular_buffer_write(
    ntt_fixed_circular_buffer_t* self,
    const uint8_t* data,
    size_t size)
{
    if (ntt_unlikely(self->len + size > self->cap)) {
        return ENOMEM;
    }

    size_t begin = self->pos + self->len;

    if (ntt_unlikely(begin >= self->cap)) {
        begin -= self->cap;
    }

    size_t end = begin + size;

    if (ntt_unlikely(self->cap < end)) {
        size_t tail = self->cap - begin;
        size_t head = end - self->cap;
        memcpy(self->data + begin, data, tail);
        memcpy(self->data, data + tail, head);
    } else {
        memcpy(self->data + begin, data, size);
    }

    self->len += size;
    return 0;
}

static inline void ntt_fixed_circular_buffer_deinit(
    ntt_fixed_circular_buffer_t* self)
{
    free(self->data);
}

static inline size_t ntt_fixed_circular_buffer_fill(
    ntt_fixed_circular_buffer_t* self,
    struct iovec* vec,
    size_t len)
{
    if (ntt_unlikely(len == 0)) {
        return 0;
    }

    size_t begin = self->pos;
    size_t end = self->pos + self->len;

    if (ntt_unlikely(self->cap < end)) {
        vec[0].iov_base = self->data + begin;
        vec[0].iov_len = self->cap - begin;

        if (ntt_unlikely(len == 1)) {
            return 1;
        }

        vec[1].iov_base = self->data;
        vec[1].iov_len = end - self->cap;
        return 2;
    }

    vec[0].iov_base = self->data + begin;
    vec[0].iov_len = self->len;
    return 1;
}

EXTERN_STOP
