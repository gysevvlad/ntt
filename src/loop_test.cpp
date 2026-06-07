#include <ntt/loop.h>
#include <ntt/queue.h>
#include <ntt/task.hpp>

#include "./loop.h"

#include <gtest/gtest.h>

#include <cassert>
#include <chrono>
#include <future>
#include <thread>

TEST(LoopTest, CreateEmpty)
{
    ntt_loop_run(
        ntt::make_task([] { std::cout << "Hello!" << std::endl; }),
        4);
}

TEST(LoopTest, StartEvent)
{
    bool was = false;
    ntt_loop_run(
        ntt::make_task([&] { was = true; }),
        4);
    ASSERT_TRUE(was);
}

TEST(LoopTest, StartSequence)
{
    static constexpr std::size_t g_task_cnt = 16;

    std::size_t cnt = 0;

    ntt_loop_run(
        ntt::make_task([&] {
            for (std::size_t i = 0; i < g_task_cnt; ++i) {
                ntt_queue_push(
                    ntt_queue_self(),
                    ntt::make_task([&] { cnt += 1; }));
            }
        }),
        4);

    ASSERT_EQ(cnt, g_task_cnt);
}

TEST(LoopTest, ExternalTasks)
{
    static constexpr std::size_t g_task_cnt = 1'000;

    std::size_t cnt = 0;
    {
        std::promise<ntt_queue_t*> promise;
        auto future = promise.get_future();

        std::jthread thread { [&] {
            ntt_loop_run(
                ntt::make_task([promise = std::move(promise)] mutable {
                    promise.set_value(ntt_queue_acquire(ntt_queue_self()));
                }),
                8);
        } };

        auto* queue = future.get();

        for (std::size_t i = 0; i < g_task_cnt; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds { 1 });
            ntt_queue_push(
                queue,
                ntt::make_task([&] {
                    cnt += 1;
                }));
        }

        ntt_queue_release(queue);
    }

    ASSERT_EQ(cnt, g_task_cnt);
}

TEST(LoopTest, BarrierTask)
{
    bool done = false;
    {
        std::promise<ntt_queue_t*> promise;
        auto future = promise.get_future();

        std::jthread thread { [&] {
            ntt_loop_run(
                ntt::make_task([promise = std::move(promise)] mutable {
                    promise.set_value(ntt_queue_acquire(ntt_queue_self()));
                }),
                8);
        } };

        auto* queue = future.get();

        std::this_thread::sleep_for(std::chrono::milliseconds { 1 });

        ntt_loop_push_barrier(
            ntt_queue_loop(queue),
            ntt::make_task([&] { done = true; }));

        ntt_queue_release(queue);
    }
    ASSERT_TRUE(done);
}
