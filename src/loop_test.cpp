#include "ntt/queue.h"
#include "ntt/task.hpp"
#include "queue.h"
#include <chrono>
#include <future>
#include <gtest/gtest.h>

#include <ntt/loop.h>
#include <thread>

TEST(LoopTest, CreateEmpty)
{
    ntt_loop_run(
        ntt_loop_cbs_t {
            .on_start = [](void* ctx, ntt_queue_t* queue) { },
        },
        nullptr, 4);
}

TEST(LoopTest, StartEvent)
{
    struct Context {
        ntt_queue_t* from_args = nullptr;
        ntt_queue_t* from_self = nullptr;
    } context;

    ntt_loop_run(
        ntt_loop_cbs_t {
            .on_start = [](void* ctx, ntt_queue_t* queue) {
                auto& context = *static_cast<Context*>(ctx);

                context.from_args = queue;
                context.from_self = ntt_queue_self();
            },
        },
        &context, 4);

    ASSERT_NE(context.from_args, nullptr);
    ASSERT_NE(context.from_self, nullptr);
    ASSERT_EQ(context.from_args, context.from_self);
}

TEST(LoopTest, StartSequence)
{
    static constexpr std::size_t g_task_cnt = 16;

    struct Context {
        std::size_t cnt = 0;
    } context;

    ntt_loop_run(
        ntt_loop_cbs_t {
            .on_start = [](void* ctx, ntt_queue_t* queue) {
                auto& context = *static_cast<Context*>(ctx);
                for (std::size_t i = 0; i < g_task_cnt; ++i) {
                    ntt_queue_push_task(
                        queue,
                        ntt::make_task([&] { context.cnt += 1; }));
                }
            },
        },
        &context, 4);

    ASSERT_EQ(context.cnt, g_task_cnt);
}

TEST(LoopTest, ExternalTasks)
{
    static constexpr std::size_t g_task_cnt = 1'000;

    struct Context {
        std::promise<ntt_queue_t*> promise;
    } context;

    std::size_t cnt = 0;
    {
        auto future = context.promise.get_future();

        std::jthread thread { [&] {
            ntt_loop_run(
                ntt_loop_cbs_t {
                    .on_start = [](void* ctx, ntt_queue_t* queue) {
                        auto& context = *static_cast<Context*>(ctx);
                        context.promise.set_value(ntt_queue_acquire(queue));
                    },
                },
                &context, 8);
        } };

        auto* queue = future.get();

        for (std::size_t i = 0; i < g_task_cnt; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds { 1 });
            ntt_queue_push_task(
                queue,
                ntt::make_task([&] {
                    cnt += 1;
                }));
        }

        ntt_queue_release(queue);
    }

    ASSERT_EQ(cnt, g_task_cnt);
}
