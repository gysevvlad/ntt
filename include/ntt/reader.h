#pragma once

#include <ntt/defs.h>
#include <ntt/loop.h>
#include <ntt/queue.h>

#include <stddef.h>
#include <stdint.h>

EXTERN_START

typedef struct ntt_reader ntt_reader_t;

typedef struct ntt_reader_vtbl {
    const char* name;

    /**
     * @brief A chunk has been read.
     *
     * Never runs on two workers at once for the same reader, and the data is
     * only valid for the call.
     */
    void (*on_data)(ntt_reader_t* self, void* ctx, uint8_t* data, size_t size);

    /**
     * @brief The reader has stopped and nothing of it is in flight.
     *
     * Comes once, on the end of the input, on a failed read, or after
     * ntt_reader_cancel. The code is zero for the end of the input and errno
     * otherwise. This is where the descriptor may be closed and the reader
     * deleted.
     */
    void (*on_cancel)(ntt_reader_t* self, void* ctx, int ec);
} ntt_reader_vtbl_t;

NTT_EXPORT ntt_reader_t* ntt_reader_create(
    const ntt_reader_vtbl_t* vtbl,
    void* ctx);

/**
 * @brief Start reading the descriptor, which has to be non blocking.
 *
 * The callbacks run in the queue, so the reader shares a context with its
 * timers and tasks. Holds the loop running until the reader stops. Call once.
 */
NTT_EXPORT void ntt_reader_start(
    ntt_reader_t* self,
    int fd,
    ntt_queue_t* queue);

/**
 * @brief Stop reading.
 *
 * May be called from any thread and more than once. The descriptor stays open
 * and must not be closed before on_cancel.
 */
NTT_EXPORT void ntt_reader_cancel(
    ntt_reader_t* self);

NTT_EXPORT void ntt_reader_delete(
    ntt_reader_t* self);

EXTERN_STOP
