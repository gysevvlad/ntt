#include <ntt/event.h>
#include <ntt/loop.h>
#include <ntt/queue.h>
#include <ntt/task.hpp>

#include "./loop.h"

#include <gtest/gtest.h>

#include <cerrno>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

namespace {

struct Probe {
    std::atomic<int> readable { 0 };
    std::atomic<int> writable { 0 };
    std::atomic<int> cancelled { 0 };

    std::atomic<int> inside { 0 };
    std::atomic<int> most_inside { 0 };

    std::atomic<bool> entered { false };
    std::atomic<bool> hold { false };

    std::atomic<std::uint64_t> drained { 0 };
    std::atomic<unsigned> first_side { 0 };
    std::atomic<unsigned> every_side { 0 };

    /** Read the descriptor dry in the handler. */
    bool drains { true };

    /** Cancel the event once this many notifications have been drained. */
    std::uint64_t cancel_at { 0 };

    int fd { -1 };
    ntt_event_handle_t handle { NTT_EVENT_HANDLE_NONE };
};

std::uint64_t drain(int fd)
{
    std::uint64_t total = 0;

    for (;;) {
        eventfd_t value = 0;
        if (eventfd_read(fd, &value) != 0) {
            return total;
        }
        total += value;
    }
}

void probe_side(Probe* probe, ntt_interest_t side)
{
    unsigned none = 0;
    probe->first_side.compare_exchange_strong(none, side);
    probe->every_side.fetch_or(side);
}

void probe_readable(ntt_event_handle_t self, void* ctx, int fd)
{
    auto* probe = static_cast<Probe*>(ctx);

    probe->readable.fetch_add(1);
    probe_side(probe, NTT_INTEREST_READABLE);

    int inside = probe->inside.fetch_add(1) + 1;
    for (int seen = probe->most_inside.load();
         seen < inside && !probe->most_inside.compare_exchange_weak(seen, inside);) {
    }

    probe->entered.store(true);

    while (probe->hold.load()) {
        std::this_thread::yield();
    }

    if (probe->drains) {
        probe->drained.fetch_add(drain(fd));
    }

    if (probe->cancel_at != 0 && probe->drained.load() >= probe->cancel_at) {
        ntt_event_cancel(ntt_loop_self(), self);
    }

    probe->inside.fetch_sub(1);
}

void probe_writable(ntt_event_handle_t, void* ctx, int)
{
    auto* probe = static_cast<Probe*>(ctx);

    probe->writable.fetch_add(1);
    probe_side(probe, NTT_INTEREST_WRITABLE);
}

void probe_cancelled(void* ctx, int fd)
{
    auto* probe = static_cast<Probe*>(ctx);

    probe->cancelled.fetch_add(1);
    close(fd);
}

const ntt_event_vtbl_t g_probe_vtbl = {
    .name      = "probe",
    .readable  = probe_readable,
    .writable  = probe_writable,
    .cancelled = probe_cancelled,
};

int make_eventfd()
{
    return eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE);
}

bool make_socketpair(int* pair)
{
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, pair)
        != 0) {
        return false;
    }

    int size = 4096;
    setsockopt(pair[0], SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
    setsockopt(pair[1], SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));

    return true;
}

template <typename Predicate>
bool wait_for(Predicate done)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds { 5 };

    while (!done()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        std::this_thread::yield();
    }

    return true;
}

} // namespace

TEST(EventTest, ReadableRunsAndSelfCancelStopsTheLoop)
{
    Probe probe;
    probe.cancel_at = 1;

    ntt_loop_run(
        ntt::make_task([&] {
            probe.fd     = make_eventfd();
            ntt_event_start(
                ntt_loop_self(),
                probe.fd,
                &g_probe_vtbl,
                &probe,
                NTT_INTEREST_READABLE,
                &probe.handle);

            eventfd_write(probe.fd, 1);
        }),
        4);

    ASSERT_GE(probe.readable.load(), 1);
    ASSERT_EQ(probe.cancelled.load(), 1);
    ASSERT_GE(probe.drained.load(), 1u);
}

TEST(EventTest, StartedEventKeepsTheLoopRunning)
{
    Probe probe;

    std::promise<ntt_loop_t*> started;
    std::promise<void> finished;

    auto loop_ready = started.get_future();
    auto loop_done  = finished.get_future();

    std::jthread thread { [&] {
        ntt_loop_run(
            ntt::make_task([&] {
                probe.fd     = make_eventfd();
                ntt_event_start(
                    ntt_loop_self(),
                    probe.fd,
                    &g_probe_vtbl,
                    &probe,
                    NTT_INTEREST_READABLE,
                    &probe.handle);

                started.set_value(ntt_loop_self());
            }),
            4);

        finished.set_value();
    } };

    ntt_loop_t* loop = loop_ready.get();

    // Nothing but the event is left, so the loop may only finish once it is
    // cancelled.
    auto held = loop_done.wait_for(std::chrono::milliseconds { 100 });

    if (held == std::future_status::timeout) {
        ntt_event_cancel(loop, probe.handle);
    }

    auto stopped = loop_done.wait_for(std::chrono::seconds { 5 });

    ASSERT_EQ(held, std::future_status::timeout);
    ASSERT_EQ(stopped, std::future_status::ready);
    ASSERT_EQ(probe.cancelled.load(), 1);
}

TEST(EventTest, CancellingTwiceReportsOnce)
{
    Probe probe;

    ntt_loop_run(
        ntt::make_task([&] {
            probe.fd     = make_eventfd();
            ntt_event_start(
                ntt_loop_self(),
                probe.fd,
                &g_probe_vtbl,
                &probe,
                NTT_INTEREST_READABLE,
                &probe.handle);

            ntt_event_cancel(ntt_loop_self(), probe.handle);
            ntt_event_cancel(ntt_loop_self(), probe.handle);
        }),
        4);

    ASSERT_EQ(probe.cancelled.load(), 1);
    ASSERT_EQ(probe.readable.load(), 0);
}

TEST(EventTest, StaleHandleDoesNotTouchTheEventThatReusedTheSlot)
{
    Probe first;
    Probe second;

    ntt_loop_run(
        ntt::make_task([&] {
            ntt_loop_t* loop = ntt_loop_self();

            first.fd     = make_eventfd();
            ntt_event_start(
                loop,
                first.fd,
                &g_probe_vtbl,
                &first,
                NTT_INTEREST_READABLE,
                &first.handle);
            ntt_event_cancel(loop, first.handle);

            second.fd     = make_eventfd();
            ntt_event_start(
                loop,
                second.fd,
                &g_probe_vtbl,
                &second,
                NTT_INTEREST_READABLE,
                &second.handle);

            // The slot is the one the first event just gave back.
            EXPECT_EQ(
                static_cast<std::uint32_t>(first.handle),
                static_cast<std::uint32_t>(second.handle));
            EXPECT_NE(first.handle, second.handle);

            ntt_event_cancel(loop, first.handle);
            EXPECT_EQ(second.cancelled.load(), 0);

            ntt_event_cancel(loop, second.handle);
        }),
        4);

    ASSERT_EQ(first.cancelled.load(), 1);
    ASSERT_EQ(second.cancelled.load(), 1);
}

TEST(EventTest, HandlersOfOneSideNeverOverlap)
{
    static constexpr std::uint64_t g_writes = 2'000;

    Probe probe;
    probe.cancel_at = g_writes;

    std::promise<int> started;
    auto fd_ready = started.get_future();

    std::jthread thread { [&] {
        ntt_loop_run(
            ntt::make_task([&] {
                probe.fd     = make_eventfd();
                ntt_event_start(
                    ntt_loop_self(),
                    probe.fd,
                    &g_probe_vtbl,
                    &probe,
                    NTT_INTEREST_READABLE,
                    &probe.handle);

                started.set_value(probe.fd);
            }),
            8);
    } };

    int fd = fd_ready.get();
    for (std::uint64_t i = 0; i < g_writes; ++i) {
        eventfd_write(fd, 1);
    }

    thread.join();

    ASSERT_EQ(probe.drained.load(), g_writes);
    ASSERT_EQ(probe.most_inside.load(), 1);
    ASSERT_EQ(probe.cancelled.load(), 1);
}

TEST(EventTest, CancellingWhileAHandlerRunsWaitsForIt)
{
    Probe probe;
    probe.hold.store(true);

    std::promise<ntt_loop_t*> started;
    auto loop_ready = started.get_future();

    std::jthread thread { [&] {
        ntt_loop_run(
            ntt::make_task([&] {
                probe.fd     = make_eventfd();
                ntt_event_start(
                    ntt_loop_self(),
                    probe.fd,
                    &g_probe_vtbl,
                    &probe,
                    NTT_INTEREST_READABLE,
                    &probe.handle);

                started.set_value(ntt_loop_self());
                eventfd_write(probe.fd, 1);
            }),
            4);
    } };

    ntt_loop_t* loop = loop_ready.get();

    bool entered = wait_for([&] { return probe.entered.load(); });

    ntt_event_cancel(loop, probe.handle);
    ntt_event_cancel(loop, probe.handle);

    // The handler still holds a reference, so the event cannot be gone yet.
    int cancelled_while_inside = probe.cancelled.load();

    probe.hold.store(false);

    thread.join();

    ASSERT_TRUE(entered);
    ASSERT_EQ(cancelled_while_inside, 0);
    ASSERT_EQ(probe.cancelled.load(), 1);
}

TEST(EventTest, TheKernelDecidesWhichSideIsReported)
{
    Probe probe;
    probe.drains = false;

    int peer = -1;

    std::promise<ntt_loop_t*> started;
    auto loop_ready = started.get_future();

    std::jthread thread { [&] {
        ntt_loop_run(
            ntt::make_task([&] {
                int pair[2] = { -1, -1 };
                EXPECT_TRUE(make_socketpair(pair));

                probe.fd = pair[0];
                peer     = pair[1];

                ntt_event_start(
                    ntt_loop_self(),
                    probe.fd,
                    &g_probe_vtbl,
                    &probe,
                    static_cast<ntt_interest_t>(
                        NTT_INTEREST_READABLE | NTT_INTEREST_WRITABLE),
                     &probe.handle);

                started.set_value(ntt_loop_self());
            }),
            4);
    } };

    ntt_loop_t* loop = loop_ready.get();

    // A fresh socket is writable and has nothing to read, and that is the side
    // the handler has to hear about, although both were subscribed to.
    bool reported  = wait_for([&] { return probe.first_side.load() != 0; });
    unsigned first = probe.first_side.load();

    ssize_t written = write(peer, "x", 1);
    bool saw_read   = wait_for([&] {
        return (probe.every_side.load() & NTT_INTEREST_READABLE) != 0;
    });

    ntt_event_cancel(loop, probe.handle);
    thread.join();
    close(peer);

    ASSERT_TRUE(reported);
    ASSERT_EQ(first, unsigned { NTT_INTEREST_WRITABLE });
    ASSERT_EQ(written, 1);
    ASSERT_TRUE(saw_read);
    ASSERT_EQ(probe.cancelled.load(), 1);
}

namespace {

/**
 * @brief A socket whose send buffer is full while its reader is busy.
 *
 * The reader holds its side for as long as the test wants, so a writable
 * handler that runs meanwhile proves the two sides do not wait for each other.
 */
struct Pipeline {
    std::atomic<bool> filled { false };
    std::atomic<bool> reading { false };
    std::atomic<bool> hold { true };
    std::atomic<int> wrote_while_reading { 0 };
    std::atomic<int> cancelled { 0 };

    ntt_event_handle_t handle { NTT_EVENT_HANDLE_NONE };
};

void pipeline_readable(ntt_event_handle_t, void* ctx, int)
{
    auto* self = static_cast<Pipeline*>(ctx);

    self->reading.store(true);

    while (self->hold.load()) {
        std::this_thread::yield();
    }

    self->reading.store(false);
}

void pipeline_writable(ntt_event_handle_t, void* ctx, int fd)
{
    auto* self = static_cast<Pipeline*>(ctx);

    if (!self->filled.load()) {
        static const char g_block[1024] = {};

        for (;;) {
            if (write(fd, g_block, sizeof(g_block)) >= 0) {
                continue;
            }
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        self->filled.store(true);
        return;
    }

    if (self->reading.load()) {
        self->wrote_while_reading.fetch_add(1);
    }
}

void pipeline_cancelled(void* ctx, int fd)
{
    static_cast<Pipeline*>(ctx)->cancelled.fetch_add(1);
    close(fd);
}

const ntt_event_vtbl_t g_pipeline_vtbl = {
    .name      = "pipeline",
    .readable  = pipeline_readable,
    .writable  = pipeline_writable,
    .cancelled = pipeline_cancelled,
};

} // namespace

TEST(EventTest, ReadAndWriteOfOneSocketRunAtTheSameTime)
{
    Pipeline pipeline;

    int peer = -1;

    std::promise<ntt_loop_t*> started;
    auto loop_ready = started.get_future();

    std::jthread thread { [&] {
        ntt_loop_run(
            ntt::make_task([&] {
                int pair[2] = { -1, -1 };
                EXPECT_TRUE(make_socketpair(pair));

                peer = pair[1];

                ntt_event_start(
                    ntt_loop_self(),
                    pair[0],
                    &g_pipeline_vtbl,
                    &pipeline,
                    static_cast<ntt_interest_t>(
                        NTT_INTEREST_READABLE | NTT_INTEREST_WRITABLE),
                     &pipeline.handle);

                started.set_value(ntt_loop_self());
            }),
            4);
    } };

    ntt_loop_t* loop = loop_ready.get();

    // The send buffer is full, so writability will only come back when the
    // peer reads.
    bool filled = wait_for([&] { return pipeline.filled.load(); });

    // Readability, which the handler holds on to without reading.
    ssize_t written = filled ? write(peer, "x", 1) : -1;
    bool reading = written == 1
        && wait_for([&] { return pipeline.reading.load(); });

    if (reading) {
        // Room to send again, while the reader is still inside its handler.
        for (;;) {
            char block[1024];
            if (read(peer, block, sizeof(block)) >= 0) {
                continue;
            }
            if (errno == EINTR) {
                continue;
            }
            break;
        }
    }

    bool wrote = reading
        && wait_for([&] { return pipeline.wrote_while_reading.load() > 0; });

    pipeline.hold.store(false);
    ntt_event_cancel(loop, pipeline.handle);
    thread.join();
    close(peer);

    ASSERT_TRUE(filled);
    ASSERT_EQ(written, 1);
    ASSERT_TRUE(reading);
    ASSERT_TRUE(wrote);
    ASSERT_EQ(pipeline.cancelled.load(), 1);
}

namespace {

/**
 * @brief An owner its own handler is allowed to free.
 *
 * It cancels through the handle ntt_event_start stored in it, which is only
 * sound because that store happens before the event can fire.
 */
struct Owner {
    ntt_event_handle_t handle { NTT_EVENT_HANDLE_NONE };

    std::atomic<int>* stored_before_firing { nullptr };
    std::atomic<int>* cancelled { nullptr };
};

void owner_readable(ntt_event_handle_t self, void* ctx, int fd)
{
    auto* owner = static_cast<Owner*>(ctx);

    drain(fd);
    owner->stored_before_firing->store(owner->handle == self ? 1 : 0);

    // Through the handle of the call, so that a handle stored too late fails
    // the test instead of leaving the loop running forever.
    ntt_event_cancel(ntt_loop_self(), self);
}

void owner_cancelled(void* ctx, int fd)
{
    auto* owner = static_cast<Owner*>(ctx);

    owner->cancelled->fetch_add(1);
    close(fd);

    delete owner;
}

const ntt_event_vtbl_t g_owner_vtbl = {
    .name      = "owner",
    .readable  = owner_readable,
    .writable  = nullptr,
    .cancelled = owner_cancelled,
};

} // namespace

TEST(EventTest, OwnerMayBeFreedByItsOwnHandler)
{
    std::atomic<int> stored_before_firing { -1 };
    std::atomic<int> cancelled { 0 };

    ntt_loop_run(
        ntt::make_task([&] {
            auto* owner                 = new Owner;
            owner->stored_before_firing = &stored_before_firing;
            owner->cancelled            = &cancelled;

            int fd = make_eventfd();

            // Ready before the registration, so a worker can run the handler,
            // cancel the event and free the owner while the call below is
            // still returning.
            eventfd_write(fd, 1);

            ntt_event_start(
                ntt_loop_self(),
                fd,
                &g_owner_vtbl,
                owner,
                NTT_INTEREST_READABLE,
                &owner->handle);
        }),
        4);

    ASSERT_EQ(stored_before_firing.load(), 1);
    ASSERT_EQ(cancelled.load(), 1);
}
