#pragma once

#include "ntt/writer.h"

#include <pthread.h>

#include <sys/uio.h>

// recommended sizes
#define NTT_TX_CHUNK_SIZE (1 << 12)
#define NTT_TX_CHUNK_CNT ((size_t)1 << (size_t)9)

typedef struct ntt_fixed_circular_buffer {
    size_t cap;
    size_t pos;
    size_t len;
    uint8_t* data;
} ntt_fixed_circular_buffer_t;

void ntt_fixed_circular_buffer_init(
    ntt_fixed_circular_buffer_t* self,
    size_t cap);

int ntt_fixed_circular_buffer_write(
    ntt_fixed_circular_buffer_t* self,
    const uint8_t* data,
    size_t size);

void ntt_fixed_circular_buffer_deinit(
    ntt_fixed_circular_buffer_t* self);

size_t ntt_fixed_circular_buffer_fill(
    ntt_fixed_circular_buffer_t* self,
    struct iovec* vec,
    size_t len);

struct ntt_writer {
    ntt_pool_t* pool;
    int fd;
    const ntt_writer_listener_tbl_t* listener_tbl;
    void* listener_ctx;
    pthread_mutex_t mtx;
};

void ntt_writer_init(
    ntt_writer_t* self,
    ntt_pool_t* pool,
    int fd,
    const ntt_writer_listener_tbl_t* writer_tbl,
    void* ctx);

void ntt_writer_deinit(
    ntt_writer_t* self);
