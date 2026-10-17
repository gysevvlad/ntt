#pragma once

#include "ntt/defs.h"
#include "ntt/loop.h"
#include "ntt/queue.h"

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
 * Each side runs in the queue it was given, so a handler is serialized with
 * the timers and the tasks of that queue. Two sides given different queues may
 * therefore run at the same time, and whatever they share needs guarding; two
 * sides given the same queue never do. A side on its own never runs twice at
 * once. The callback of a side that was not subscribed to may be left out.
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
     * @brief The writable side is gone and nothing of it is in flight.
     *
     * Runs in the queue of that side, so this is where the owner releases what
     * belongs to writing. Comes before the readable side is reported, and is
     * the last word only when there is no readable side. Optional.
     */
    void (*writable_cancelled)(void* ctx, int fd);

    /**
     * @brief The readable side is gone and nothing of it is in flight.
     *
     * Runs in the queue of that side, and after the writable side has been
     * reported, so for an event that reads this is the last word: nothing of
     * it is left, and this is where the owner closes the descriptor and
     * releases its context.
     */
    void (*readable_cancelled)(void* ctx, int fd);
} ntt_event_vtbl_t;

/**
 * @brief What an event is started with.
 */
typedef struct ntt_event_params {
    /** Descriptor to watch, which has to be non blocking. */
    int fd;

    const ntt_event_vtbl_t* vtbl;
    void* ctx;

    /** Sides to watch. */
    ntt_interest_t interest;

    /**
     * @brief Queues the sides run in, one each, of the same loop.
     *
     * A side that is subscribed to needs one. Giving both sides the same queue
     * serializes the whole descriptor, which is the simplest thing to reason
     * about; giving them different ones lets reading and writing proceed at
     * the same time, at the price of guarding whatever they share.
     */
    ntt_queue_t* read_queue;
    ntt_queue_t* write_queue;

    /**
     * @brief Where to store the handle of the event.
     *
     * Stored before the descriptor is registered, which is the only way an
     * owner can keep it: once the event can fire, a handler may already have
     * cancelled it and the cancellation may already have freed the owner. May
     * be NULL when nobody keeps the handle, since every handler is given it
     * anyway.
     */
    ntt_event_handle_t* handle;
} ntt_event_params_t;

/**
 * @brief Start watching the descriptor.
 *
 * The event keeps a reference to every queue it was given, and holds the loop
 * running, until it is cancelled.
 */
NTT_EXPORT void ntt_event_start(const ntt_event_params_t* params);

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
