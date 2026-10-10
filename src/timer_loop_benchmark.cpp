#include <ntt/loop.h>
#include <ntt/queue.h>
#include <ntt/task.hpp>
#include <ntt/timer.h>

#include <benchmark/benchmark.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>

namespace {

using clock_type = std::chrono::steady_clock;

constexpr std::uint64_t g_ms = 1000 * 1000;

/**
 * @brief State of one queue driving a stream of requests.
 *
 * Lives on the heap, so the tasks of the queue only carry a pointer and fit
 * in the payload of a task.
 */
struct Driver {
    std::atomic<int>* done;
    int left;
    int depth;
    std::uint64_t timeout;
    std::deque<ntt_timer_tag_t> in_flight;
};

void drive(Driver* self);

/**
 * @brief One request: answer the oldest one, then send a new one.
 *
 * The answer comes first on purpose. With depth 1 that empties the local heap
 * of the queue, so it deregisters from the timer service and registers again
 * on the next request: the shared lock of the service is taken twice per
 * request. With a larger depth the heap never empties and the registration
 * stays untouched.
 */
void step(Driver* self)
{
    if (static_cast<int>(self->in_flight.size()) >= self->depth) {
        ntt_self_cancel_timer(self->in_flight.front());
        self->in_flight.pop_front();
    }

    self->in_flight.push_back(
        ntt_self_wakeup_after(self->timeout, ntt::make_task([] { })));
}

void finish(Driver* self)
{
    // Nothing is allowed to stay armed: the deadlines are far away and would
    // keep the loop running.
    for (ntt_timer_tag_t tag : self->in_flight) {
        ntt_self_cancel_timer(tag);
    }

    self->done->fetch_add(1);

    delete self;
}

void drive(Driver* self)
{
    if (self->left == 0) {
        finish(self);
        return;
    }

    self->left -= 1;

    step(self);

    ntt_queue_push(ntt_queue_self(), ntt::make_task([self] { drive(self); }));
}

/**
 * @brief Timers that actually fire, re-armed by their own callback.
 */
struct Ticker {
    std::atomic<int>* done;
    int left;
};

void tick(Ticker state)
{
    if (state.left == 0) {
        state.done->fetch_add(1);
        return;
    }

    ntt_self_wakeup_at(
        ntt_now_ns(),
        ntt::make_task([state] { tick(Ticker { state.done, state.left - 1 }); }));
}

} // namespace

/**
 * @brief Requests with a deadline that is almost always cancelled.
 *
 * Args: queues / in flight per queue / requests per queue / worker threads.
 */
static void BM_LoopDeadlines(benchmark::State& state)
{
    const int queues   = static_cast<int>(state.range(0));
    const int depth    = static_cast<int>(state.range(1));
    const int rounds   = static_cast<int>(state.range(2));
    const unsigned width = static_cast<unsigned>(state.range(3));

    for (auto _ : state) {
        std::atomic<int> done { 0 };

        auto start = clock_type::now();

        ntt_loop_run(
            ntt::make_task([&] {
                for (int i = 0; i < queues; ++i) {
                    ntt_queue_t* queue = ntt_queue_create();

                    auto* driver = new Driver { &done, rounds, depth, 60ull * 1000 * g_ms, {} };

                    ntt_queue_push(queue, ntt::make_task([driver] { drive(driver); }));
                    ntt_queue_release(queue);
                }
            }),
            width);

        auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
            clock_type::now() - start);

        if (done.load() != queues) {
            state.SkipWithError("not every queue finished");
            break;
        }

        state.SetIterationTime(elapsed.count());
    }

    state.SetItemsProcessed(
        state.iterations() * static_cast<std::int64_t>(queues) * rounds);
}

BENCHMARK(BM_LoopDeadlines)
    ->UseManualTime()
    ->Args({ 256, 1, 100, 1 })
    ->Args({ 256, 1, 100, 2 })
    ->Args({ 256, 1, 100, 4 })
    ->Args({ 256, 1, 100, 8 })
    ->Args({ 256, 8, 100, 8 })
    ->Args({ 256, 64, 100, 8 })
    ->Args({ 2048, 1, 100, 8 })
    ->Args({ 2048, 8, 100, 8 });

/**
 * @brief Timers that fire, so the whole timerfd path is measured.
 *
 * Args: queues / wakeups per queue / worker threads.
 */
static void BM_LoopWakeups(benchmark::State& state)
{
    const int queues     = static_cast<int>(state.range(0));
    const int rounds     = static_cast<int>(state.range(1));
    const unsigned width = static_cast<unsigned>(state.range(2));

    for (auto _ : state) {
        std::atomic<int> done { 0 };

        auto start = clock_type::now();

        ntt_loop_run(
            ntt::make_task([&] {
                for (int i = 0; i < queues; ++i) {
                    ntt_queue_t* queue = ntt_queue_create();

                    ntt_queue_push(
                        queue,
                        ntt::make_task([&done, rounds] {
                            tick(Ticker { &done, rounds });
                        }));

                    ntt_queue_release(queue);
                }
            }),
            width);

        auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
            clock_type::now() - start);

        if (done.load() != queues) {
            state.SkipWithError("not every queue finished");
            break;
        }

        state.SetIterationTime(elapsed.count());
    }

    state.SetItemsProcessed(
        state.iterations() * static_cast<std::int64_t>(queues) * rounds);
}

BENCHMARK(BM_LoopWakeups)
    ->UseManualTime()
    ->Args({ 64, 50, 1 })
    ->Args({ 64, 50, 2 })
    ->Args({ 64, 50, 4 })
    ->Args({ 64, 50, 8 })
    ->Args({ 512, 50, 8 });
