#pragma once

#include "ntt/defs.h"
#include "ntt/loop.h"

#include <stdint.h>

EXTERN_START

typedef enum ntt_interest {
    NTT_INTEREST_READABLE = 0b01,
    NTT_INTEREST_WRITABLE = 0b10,
} ntt_interest_t;

/**
 * @brief Handle of a started event.
 *
 * The library owns the event, so what the caller keeps is a handle and not a
 * pointer: using one after the event has stopped is harmless.
 */
typedef uint64_t ntt_event_handle_t;

#define NTT_EVENT_HANDLE_NONE ((ntt_event_handle_t)0)

/**
 * @brief Callbacks of an event.
 *
 * The two sides of a descriptor are independent: a readable handler and a
 * writable handler of the same event may run at the same time on two workers,
 * so whatever they share needs guarding, while each side on its own never runs
 * twice at once. The callback of a side that was not subscribed to may be left
 * out.
 */
typedef struct ntt_event_vtbl {
    const char* name;

    /**
     * @brief There is something to read, or reading will report the failure.
     *
     * An error or a hangup on the descriptor comes as readiness of every
     * direction subscribed to, so that the handler meets it as a failing read
     * or write.
     *
     * Runs again right away if a notification arrived while it was running,
     * and never after ntt_event_cancel has returned.
     */
    void (*readable)(ntt_event_handle_t self, void* ctx, int fd);

    /**
     * @brief There is room to write, or writing will report the failure.
     */
    void (*writable)(ntt_event_handle_t self, void* ctx, int fd);

    /**
     * @brief The event is gone and nothing of it is in flight any more.
     *
     * Runs once, after the last handler of either side has returned. This is
     * where the owner closes the descriptor and releases its context.
     */
    void (*cancelled)(void* ctx, int fd);
} ntt_event_vtbl_t;

/**
 * @brief Start watching the descriptor in the loop.
 *
 * Holds the loop running until the event is cancelled.
 *
 * The handle is not returned but stored, and stored before the descriptor is
 * registered, which is the only way an owner can keep it: once the event can
 * fire, a handler may already have cancelled it and the cancellation may
 * already have freed the owner. May be NULL when nobody keeps the handle,
 * since every handler is given it anyway.
 */
NTT_EXPORT void ntt_event_start(
    ntt_loop_t* loop,
    int fd,
    const ntt_event_vtbl_t* vtbl,
    void* ctx,
    ntt_interest_t interest,
    ntt_event_handle_t* handle);

/**
 * @brief Stop watching the descriptor and release the event.
 *
 * May be called from any thread, more than once, and from inside a handler:
 * the cancelled callback comes when the last handler in flight returns, so a
 * handler that cancels its own event is still free to touch the descriptor
 * until it returns.
 */
NTT_EXPORT void ntt_event_cancel(ntt_loop_t* loop, ntt_event_handle_t handle);

EXTERN_STOP
