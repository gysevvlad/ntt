#include <ntt/loop.h>
#include <ntt/queue.h>
#include <ntt/task.hpp>
#include <ntt/timer.h>

#include "./queue.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <vector>

namespace {

using clock_type = std::chrono::steady_clock;

constexpr std::uint64_t g_ms = 1000 * 1000;

std::uint64_t elapsed_ms(clock_type::time_point start)
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            clock_type::now() - start)
            .count());
}

} // namespace

TEST(TimerTest, WakeupFires)
{
    bool fired = false;

    auto start = clock_type::now();

    ntt_loop_run(
        ntt::make_task([&] {
            ntt_self_wakeup_after(20 * g_ms, ntt::make_task([&] { fired = true; }));
        }),
        4);

    ASSERT_TRUE(fired);
    ASSERT_GE(elapsed_ms(start), 20u);
}

TEST(TimerTest, LoopStaysAliveWhileTimerIsArmed)
{
    // The loop has nothing else to do, so the only thing keeping it running is
    // the armed timer.
    auto start = clock_type::now();

    ntt_loop_run(
        ntt::make_task([&] {
            ntt_self_wakeup_after(50 * g_ms, ntt::make_task([] { }));
        }),
        2);

    ASSERT_GE(elapsed_ms(start), 50u);
}

TEST(TimerTest, CancelledTimerDoesNotFire)
{
    bool fired = false;

    auto start = clock_type::now();

    ntt_loop_run(
        ntt::make_task([&] {
            auto tag = ntt_self_wakeup_after(
                30ull * 1000 * g_ms, // 30 seconds
                ntt::make_task([&] { fired = true; }));
            ntt_self_cancel_timer(tag);
        }),
        2);

    ASSERT_FALSE(fired);

    // Cancelling drops the registration, so the loop must not wait for the
    // original deadline.
    ASSERT_LT(elapsed_ms(start), 2000u);
}

TEST(TimerTest, FiresInItsOwnQueue)
{
    ntt_queue_t* armed_in = nullptr;
    ntt_queue_t* fired_in = nullptr;

    ntt_loop_run(
        ntt::make_task([&] {
            armed_in = ntt_queue_self();
            ntt_self_wakeup_after(
                10 * g_ms,
                ntt::make_task([&] { fired_in = ntt_queue_self(); }));
        }),
        4);

    ASSERT_NE(armed_in, nullptr);
    ASSERT_EQ(fired_in, armed_in);
}

TEST(TimerTest, SeveralTimersFireInOrder)
{
    std::vector<int> fired;

    ntt_loop_run(
        ntt::make_task([&] {
            for (int i = 5; i >= 1; --i) {
                ntt_self_wakeup_after(
                    static_cast<std::uint64_t>(i) * 10 * g_ms,
                    ntt::make_task([&fired, i] { fired.push_back(i); }));
            }
        }),
        4);

    ASSERT_EQ(fired, (std::vector<int> { 1, 2, 3, 4, 5 }));
}

TEST(TimerTest, TimerArmsAnotherTimer)
{
    int ticks = 0;

    struct Periodic {
        int* ticks;
        void operator()() const
        {
            *ticks += 1;
            if (*ticks < 3) {
                ntt_self_wakeup_after(5 * g_ms, ntt::make_task(Periodic { ticks }));
            }
        }
    };

    ntt_loop_run(
        ntt::make_task([&] {
            ntt_self_wakeup_after(5 * g_ms, ntt::make_task(Periodic { &ticks }));
        }),
        2);

    ASSERT_EQ(ticks, 3);
}

TEST(TimerTest, TimerCancelsAnotherTimer)
{
    bool first  = false;
    bool victim = false;

    ntt_loop_run(
        ntt::make_task([&] {
            auto tag = ntt_self_wakeup_after(
                20 * g_ms,
                ntt::make_task([&] { victim = true; }));

            ntt_self_wakeup_after(
                10 * g_ms,
                ntt::make_task([&, tag] {
                    first = true;
                    ntt_self_cancel_timer(tag);
                }));
        }),
        4);

    ASSERT_TRUE(first);
    ASSERT_FALSE(victim);
}

TEST(TimerTest, TimersOfSeveralQueues)
{
    static constexpr int g_queues = 16;

    std::atomic<int> fired { 0 };

    ntt_loop_run(
        ntt::make_task([&] {
            for (int i = 0; i < g_queues; ++i) {
                ntt_queue_t* queue = ntt_queue_create();
                ASSERT_NE(queue, nullptr);

                ntt_queue_push(
                    queue,
                    ntt::make_task([&fired, i] {
                        ntt_self_wakeup_after(
                            static_cast<std::uint64_t>(5 + i) * g_ms,
                            ntt::make_task([&fired] { fired.fetch_add(1); }));
                    }));

                // The queue stays alive through its pending task and then
                // through its registration in the timer service.
                ntt_queue_release(queue);
            }
        }),
        4);

    ASSERT_EQ(fired.load(), g_queues);
}

TEST(TimerTest, ManyTimersInOneQueue)
{
    static constexpr int g_cnt = 2000;

    int fired = 0;

    ntt_loop_run(
        ntt::make_task([&] {
            for (int i = 0; i < g_cnt; ++i) {
                ntt_self_wakeup_after(
                    static_cast<std::uint64_t>(1 + (i % 20)) * g_ms,
                    ntt::make_task([&fired] { fired += 1; }));
            }
        }),
        4);

    ASSERT_EQ(fired, g_cnt);
}

TEST(TimerTest, MostTimersCancelled)
{
    static constexpr int g_cnt = 2000;

    int fired = 0;

    ntt_loop_run(
        ntt::make_task([&] {
            std::vector<ntt_timer_tag_t> tags;
            for (int i = 0; i < g_cnt; ++i) {
                tags.push_back(ntt_self_wakeup_after(
                    30ull * 1000 * g_ms,
                    ntt::make_task([&fired] { fired += 1; })));
            }

            // Cancel all but the last one, which also exercises compaction.
            for (std::size_t i = 0; i + 1 < tags.size(); ++i) {
                ntt_self_cancel_timer(tags[i]);
            }

            ntt_self_cancel_timer(tags.back());

            ntt_self_wakeup_after(10 * g_ms, ntt::make_task([&fired] { fired += 1; }));
        }),
        4);

    ASSERT_EQ(fired, 1);
}

TEST(TimerTest, ImmediateWakeupsFromManyQueues)
{
    // Deadlines that have already passed, so the service arms the timerfd in
    // the past and pops the queue while its own context is still registering.
    static constexpr int g_queues = 64;
    static constexpr int g_rounds = 20;

    std::atomic<int> fired { 0 };

    struct Again {
        std::atomic<int>* fired;
        int left;

        void operator()() const
        {
            fired->fetch_add(1);
            if (left > 0) {
                ntt_self_wakeup_at(
                    ntt_now_ns(),
                    ntt::make_task(Again { fired, left - 1 }));
            }
        }
    };

    ntt_loop_run(
        ntt::make_task([&] {
            for (int i = 0; i < g_queues; ++i) {
                ntt_queue_t* queue = ntt_queue_create();

                ntt_queue_push(
                    queue,
                    ntt::make_task([&fired] {
                        ntt_self_wakeup_at(
                            ntt_now_ns(),
                            ntt::make_task(Again { &fired, g_rounds - 1 }));
                    }));

                ntt_queue_release(queue);
            }
        }),
        8);

    ASSERT_EQ(fired.load(), g_queues * g_rounds);
}

TEST(TimerTest, NowIsMonotonic)
{
    std::uint64_t a = ntt_now_ns();
    std::uint64_t b = ntt_now_ns();

    ASSERT_GE(b, a);
    ASSERT_GT(a, 0u);
}
