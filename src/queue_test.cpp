#include "./queue.h"
#include "ntt/task.hpp"

#include <gtest/gtest.h>

TEST(QueueTest, Common)
{
    struct Context {
        bool wakeup        = false;
        ntt_queue_t* queue = NULL;
    } ctx;

    ntt_queue_t queue;

    ntt_queue_init(
        &queue,
        &ctx,
        +[](void* ctx, [[maybe_unused]] ntt_queue_t* queue) {
            auto& context  = *static_cast<Context*>(ctx);
            context.wakeup = true;
            context.queue  = queue;
        },
        nullptr);

    int i = 0;

    ntt_queue_push(
        &queue,
        ntt::make_task([&] { ++i; }));

    ASSERT_TRUE(ctx.wakeup);
    ASSERT_EQ(ctx.queue, &queue);

    ASSERT_EQ(ntt_queue_svc_with_limit(&queue, 128), 1);
    ASSERT_EQ(i, 1);
}
