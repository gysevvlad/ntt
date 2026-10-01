#include "ntt/task.h"
#include "ntt/task.hpp"

#include <gtest/gtest.h>

#include <future>

class TaskTest : public testing::Test {
};

TEST_F(TaskTest, RawTask)
{
    int v     = 0;
    auto task = ntt_task_create(
        +[](void* payload, ntt_task_action_t action) {
            if (action != NTT_TASK_RUN) {
                return;
            }
            **static_cast<int**>(payload) = 1234;
        });
    *static_cast<int**>(task) = &v;
    ntt_task_svc(task);
    ntt_task_destroy(task);
    ASSERT_EQ(v, 1234);
}

TEST_F(TaskTest, Common)
{
    std::promise<int> promise;
    auto future = promise.get_future();
    auto* task  = ntt::make_task(
        [promise = std::move(promise)] mutable { promise.set_value(9); });
    ntt_task_svc(task);
    ntt_task_destroy(task);
    ASSERT_EQ(future.get(), 9);
}

TEST_F(TaskTest, DoTask)
{
    int v      = 0;
    auto* task = ntt::make_task([&] { v = 1; });
    ntt_task_svc(task);
    ntt_task_destroy(task);
    EXPECT_EQ(v, 1);
}

namespace {

struct Tracer {
    static int alive;
    Tracer() { alive += 1; }
    Tracer(const Tracer&) { alive += 1; }
    Tracer(Tracer&&) noexcept { alive += 1; }
    ~Tracer() { alive -= 1; }
};

int Tracer::alive = 0;

} // namespace

TEST_F(TaskTest, RunDestroysPayloadOnDestroy)
{
    Tracer::alive = 0;

    bool ran = false;
    {
        Tracer tracer;
        auto* task = ntt::make_task([tracer, &ran] {
            (void)tracer;
            ran = true;
        });

        ASSERT_EQ(Tracer::alive, 2); // the local one and the captured copy

        ntt_task_svc(task);
        ASSERT_TRUE(ran);
        ASSERT_EQ(Tracer::alive, 2); // svc does not destroy the payload

        ntt_task_destroy(task);
        ASSERT_EQ(Tracer::alive, 1); // only the local one is left
    }

    ASSERT_EQ(Tracer::alive, 0);
}

TEST_F(TaskTest, DestroyWithoutRunDestroysPayload)
{
    Tracer::alive = 0;

    bool ran = false;
    {
        Tracer tracer;
        auto* task = ntt::make_task([tracer, &ran] {
            (void)tracer;
            ran = true;
        });

        ASSERT_EQ(Tracer::alive, 2);

        ntt_task_destroy(task);

        ASSERT_FALSE(ran);
        ASSERT_EQ(Tracer::alive, 1);
    }

    ASSERT_EQ(Tracer::alive, 0);
}

template <class T>
bool is_aligned(const void* ptr) noexcept
{
    auto iptr = reinterpret_cast<std::uintptr_t>(ptr);
    return !(iptr % alignof(T));
}

TEST_F(TaskTest, DISABLED_CheckDefaultMallocAlignment)
{
    struct alignas(16) A {
        int a;
    };
    for (std::size_t j = 0; j < 100'000; ++j) {
        for (std::size_t i = 16; i < 256; ++i) {
            ASSERT_TRUE(is_aligned<A>(malloc(i)));
        }
    }
}

TEST_F(TaskTest, Alignment)
{
    struct alignas(8) A {
        std::uint64_t a;
    } a;
    // struct alignas(16) B {
    //   std::uint64_t a;
    // } b;

    // TODO: over-aligned task payload data
    // struct alignas(32) C {
    //   std::uint64_t a;
    // } c;

    auto* task_a = ntt::make_task([a] { });
    EXPECT_TRUE(is_aligned<A>(task_a));
    ntt_task_destroy(task_a);

    // auto *task_b = ntt::make_task([b] {});
    // EXPECT_TRUE(is_aligned<B>(task_a));
    // ntt::free_task(task_a);

    // TODO: over-aligned task payload data
    // auto *task_c = ntt::make_task([c] {});
    // EXPECT_TRUE(is_aligned<C>(task_a));
    // ntt::free_task(task_a);
}
