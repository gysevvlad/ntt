#include "./task.h"
#include "./timer_heap.h"
#include "ntt/task.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

namespace {

class TimerHeapTest : public testing::Test {
protected:
    void SetUp() override { ntt_timer_heap_init(&heap); }
    void TearDown() override { ntt_timer_heap_deinit(&heap); }

    ntt_timer_tag_t push(std::uint64_t deadline, int id)
    {
        return ntt_timer_heap_push(
            &heap,
            deadline,
            ntt::make_task([this, id] { fired.push_back(id); }));
    }

    ntt_timer_heap_t heap {};
    std::vector<int> fired;
};

} // namespace

TEST_F(TimerHeapTest, Empty)
{
    std::uint64_t deadline = 777;

    ASSERT_EQ(ntt_timer_heap_empty(&heap), 1);
    ASSERT_EQ(ntt_timer_heap_size(&heap), 0u);
    ASSERT_EQ(ntt_timer_heap_peek(&heap, &deadline), 0);
    ASSERT_EQ(deadline, 777u);
    ASSERT_EQ(ntt_timer_heap_svc(&heap, UINT64_MAX), 0u);
}

TEST_F(TimerHeapTest, PushDoesNotAllocateUntilFirstTimer)
{
    ASSERT_EQ(heap.heap_block_cnt, 0u);
    ASSERT_EQ(heap.slot_block_cnt, 0u);

    push(10, 1);

    ASSERT_EQ(heap.heap_block_cnt, 1u);
    ASSERT_EQ(heap.slot_block_cnt, 1u);
}

TEST_F(TimerHeapTest, PeekReturnsEarliest)
{
    push(30, 3);
    push(10, 1);
    push(20, 2);

    std::uint64_t deadline = 0;
    ASSERT_EQ(ntt_timer_heap_peek(&heap, &deadline), 1);
    ASSERT_EQ(deadline, 10u);
    ASSERT_EQ(ntt_timer_heap_size(&heap), 3u);
}

TEST_F(TimerHeapTest, TagIsNeverNull)
{
    ASSERT_EQ(ntt_timer_tag_is_null(ntt_timer_tag_null()), 1);
    ASSERT_EQ(ntt_timer_tag_is_null(push(10, 1)), 0);
}

TEST_F(TimerHeapTest, SvcRunsInDeadlineOrder)
{
    std::mt19937 gen { std::random_device {}() };

    static constexpr int g_cnt = 512;

    std::vector<int> ids(g_cnt);
    std::iota(ids.begin(), ids.end(), 0);
    std::shuffle(ids.begin(), ids.end(), gen);

    for (int id : ids) {
        push(static_cast<std::uint64_t>(id) * 10, id);
    }

    ASSERT_EQ(ntt_timer_heap_svc(&heap, UINT64_MAX), static_cast<std::size_t>(g_cnt));

    std::vector<int> expected(g_cnt);
    std::iota(expected.begin(), expected.end(), 0);
    ASSERT_EQ(fired, expected);
    ASSERT_EQ(ntt_timer_heap_empty(&heap), 1);
}

TEST_F(TimerHeapTest, EqualDeadlinesFireInStartOrder)
{
    static constexpr int g_cnt = 64;

    for (int i = 0; i < g_cnt; ++i) {
        push(1000, i);
    }

    ASSERT_EQ(ntt_timer_heap_svc(&heap, 1000), static_cast<std::size_t>(g_cnt));

    std::vector<int> expected(g_cnt);
    std::iota(expected.begin(), expected.end(), 0);
    ASSERT_EQ(fired, expected);
}

TEST_F(TimerHeapTest, StableAcrossInterleavedDeadlines)
{
    // three timers per deadline, started round robin
    for (int round = 0; round < 3; ++round) {
        for (int slot = 0; slot < 4; ++slot) {
            push(static_cast<std::uint64_t>(slot) * 100, round * 10 + slot);
        }
    }

    ntt_timer_heap_svc(&heap, UINT64_MAX);

    std::vector<int> expected {
        0, 10, 20, // deadline 0
        1, 11, 21, // deadline 100
        2, 12, 22, // deadline 200
        3, 13, 23  // deadline 300
    };
    ASSERT_EQ(fired, expected);
}

TEST_F(TimerHeapTest, SvcRunsOnlyExpired)
{
    for (int i = 1; i <= 10; ++i) {
        push(static_cast<std::uint64_t>(i), i);
    }

    ASSERT_EQ(ntt_timer_heap_svc(&heap, 5), 5u);
    ASSERT_EQ(fired, (std::vector<int> { 1, 2, 3, 4, 5 }));
    ASSERT_EQ(ntt_timer_heap_size(&heap), 5u);

    std::uint64_t deadline = 0;
    ASSERT_EQ(ntt_timer_heap_peek(&heap, &deadline), 1);
    ASSERT_EQ(deadline, 6u);

    ASSERT_EQ(ntt_timer_heap_svc(&heap, UINT64_MAX), 5u);
    ASSERT_EQ(fired, (std::vector<int> { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }));
}

TEST_F(TimerHeapTest, CancelMiddle)
{
    push(10, 1);
    auto tag = push(20, 2);
    push(30, 3);

    ntt_timer_heap_cancel(&heap, tag);

    ASSERT_EQ(ntt_timer_heap_size(&heap), 2u);
    ASSERT_EQ(ntt_timer_heap_svc(&heap, UINT64_MAX), 2u);
    ASSERT_EQ(fired, (std::vector<int> { 1, 3 }));
}

TEST_F(TimerHeapTest, CancelEarliest)
{
    auto tag = push(10, 1);
    push(20, 2);

    ntt_timer_heap_cancel(&heap, tag);

    std::uint64_t deadline = 0;
    ASSERT_EQ(ntt_timer_heap_peek(&heap, &deadline), 1);
    ASSERT_EQ(deadline, 20u);

    ntt_timer_heap_svc(&heap, UINT64_MAX);
    ASSERT_EQ(fired, (std::vector<int> { 2 }));
}

TEST_F(TimerHeapTest, CancelLast)
{
    push(10, 1);
    auto tag = push(20, 2);

    ntt_timer_heap_cancel(&heap, tag);

    ntt_timer_heap_svc(&heap, UINT64_MAX);
    ASSERT_EQ(fired, (std::vector<int> { 1 }));
}

TEST_F(TimerHeapTest, CancelOnly)
{
    auto tag = push(10, 1);

    ntt_timer_heap_cancel(&heap, tag);

    ASSERT_EQ(ntt_timer_heap_empty(&heap), 1);
    ASSERT_EQ(ntt_timer_heap_svc(&heap, UINT64_MAX), 0u);
    ASSERT_TRUE(fired.empty());
}

TEST_F(TimerHeapTest, CancelAllKeepsHeapConsistent)
{
    std::mt19937 gen { std::random_device {}() };

    static constexpr int g_cnt = 256;

    std::vector<ntt_timer_tag_t> tags;
    for (int i = 0; i < g_cnt; ++i) {
        tags.push_back(push(static_cast<std::uint64_t>(gen() % 1000), i));
    }

    std::shuffle(tags.begin(), tags.end(), gen);

    for (auto tag : tags) {
        ntt_timer_heap_cancel(&heap, tag);
    }

    ASSERT_EQ(ntt_timer_heap_empty(&heap), 1);
    ASSERT_EQ(ntt_timer_heap_svc(&heap, UINT64_MAX), 0u);
    ASSERT_TRUE(fired.empty());
}

TEST_F(TimerHeapTest, CancelledTaskIsDestroyedExactlyOnce)
{
    static int destroyed = 0;
    destroyed            = 0;

    auto* node = ntt_task_create_inl(
        +[](void*) { FAIL() << "cancelled timer must not run"; },
        +[](void* ptr) {
            destroyed += 1;
            ntt_free(ptr);
        });

    auto tag = ntt_timer_heap_push(&heap, 10, &node->payload);

    ntt_timer_heap_cancel(&heap, tag);

    ASSERT_EQ(destroyed, 1);
}

TEST_F(TimerHeapTest, StaleTagAfterFireIsIgnored)
{
    auto tag = push(10, 1);
    push(20, 2);

    ASSERT_EQ(ntt_timer_heap_svc(&heap, 10), 1u);

    // Contract violation: asserts in a debug build, ignored otherwise.
#ifdef NDEBUG
    ntt_timer_heap_cancel(&heap, tag);
    ASSERT_EQ(ntt_timer_heap_size(&heap), 1u);
    ntt_timer_heap_svc(&heap, UINT64_MAX);
    ASSERT_EQ(fired, (std::vector<int> { 1, 2 }));
#else
    (void)tag;
#endif
}

TEST_F(TimerHeapTest, SlotsAreReused)
{
    auto first = push(10, 1);
    ntt_timer_heap_cancel(&heap, first);

    auto second = push(20, 2);

    ASSERT_EQ(second.index, first.index);
    ASSERT_NE(second.version, first.version);
    ASSERT_EQ(heap.slot_cnt, 1u);
}

TEST_F(TimerHeapTest, TaskCanCancelAnotherExpiredTimer)
{
    auto victim = push(20, 2);

    ntt_timer_heap_push(
        &heap,
        10,
        ntt::make_task([&] {
            fired.push_back(1);
            ntt_timer_heap_cancel(&heap, victim);
        }));

    ASSERT_EQ(ntt_timer_heap_svc(&heap, 100), 1u);
    ASSERT_EQ(fired, (std::vector<int> { 1 }));
    ASSERT_EQ(ntt_timer_heap_empty(&heap), 1);
}

TEST_F(TimerHeapTest, TaskCanArmTimerThatRunsInSameSvc)
{
    ntt_timer_heap_push(
        &heap,
        10,
        ntt::make_task([&] {
            fired.push_back(1);
            push(50, 2);
        }));

    ASSERT_EQ(ntt_timer_heap_svc(&heap, 100), 2u);
    ASSERT_EQ(fired, (std::vector<int> { 1, 2 }));
}

TEST_F(TimerHeapTest, TaskArmingLaterTimerLeavesItArmed)
{
    ntt_timer_heap_push(
        &heap,
        10,
        ntt::make_task([&] {
            fired.push_back(1);
            push(500, 2);
        }));

    ASSERT_EQ(ntt_timer_heap_svc(&heap, 100), 1u);
    ASSERT_EQ(ntt_timer_heap_size(&heap), 1u);

    std::uint64_t deadline = 0;
    ntt_timer_heap_peek(&heap, &deadline);
    ASSERT_EQ(deadline, 500u);
}

TEST_F(TimerHeapTest, GrowsAndShrinksBlocks)
{
    static constexpr int g_cnt = NTT_TIMER_BLOCK * 3 + 7;

    for (int i = 0; i < g_cnt; ++i) {
        push(static_cast<std::uint64_t>(g_cnt - i), i);
    }

    const std::uint32_t blocks = (g_cnt + NTT_TIMER_BLOCK - 1) / NTT_TIMER_BLOCK;
    ASSERT_EQ(heap.heap_block_cnt, blocks);
    ASSERT_EQ(heap.slot_block_cnt, blocks);

    ASSERT_EQ(ntt_timer_heap_svc(&heap, UINT64_MAX), static_cast<std::size_t>(g_cnt));

    std::vector<int> expected(g_cnt);
    std::iota(expected.begin(), expected.end(), 0);
    std::reverse(expected.begin(), expected.end());
    ASSERT_EQ(fired, expected);

    // one spare heap block is kept, the slab keeps its watermark
    ASSERT_EQ(heap.heap_block_cnt, 1u);
    ASSERT_EQ(heap.slot_block_cnt, blocks);
}

TEST_F(TimerHeapTest, DeinitDestroysArmedTasks)
{
    static int destroyed = 0;
    destroyed            = 0;

    for (int i = 0; i < 3; ++i) {
        auto* node = ntt_task_create_inl(
            +[](void*) { FAIL() << "timer must not run"; },
            +[](void* ptr) {
                destroyed += 1;
                ntt_free(ptr);
            });
        ntt_timer_heap_push(&heap, 10, &node->payload);
    }

    ntt_timer_heap_deinit(&heap);

    ASSERT_EQ(destroyed, 3);
    ASSERT_EQ(ntt_timer_heap_empty(&heap), 1);

    // TearDown must tolerate a second deinit
}

TEST_F(TimerHeapTest, RandomizedAgainstReference)
{
    std::mt19937 gen { std::random_device {}() };
    std::uniform_int_distribution<std::uint64_t> deadline_dist(0, 200);

    struct Ref {
        std::uint64_t deadline;
        std::uint64_t seq;
        int id;
    };

    std::vector<Ref> ref;
    std::vector<std::pair<ntt_timer_tag_t, int>> live;
    std::uint64_t seq = 0;
    int next_id       = 0;

    for (int step = 0; step < 4000; ++step) {
        int action = static_cast<int>(gen() % 100);

        if (action < 55 || live.empty()) {
            int id            = next_id++;
            std::uint64_t dl  = deadline_dist(gen);
            live.emplace_back(push(dl, id), id);
            ref.push_back(Ref { dl, seq++, id });
        } else if (action < 85) {
            std::size_t i = gen() % live.size();
            ntt_timer_heap_cancel(&heap, live[i].first);
            int id = live[i].second;
            live.erase(live.begin() + static_cast<std::ptrdiff_t>(i));
            ref.erase(
                std::find_if(ref.begin(), ref.end(), [id](const Ref& r) { return r.id == id; }));
        } else {
            std::uint64_t now = deadline_dist(gen);

            std::vector<Ref> expired;
            for (const Ref& r : ref) {
                if (r.deadline <= now) {
                    expired.push_back(r);
                }
            }
            std::sort(expired.begin(), expired.end(), [](const Ref& a, const Ref& b) {
                return a.deadline != b.deadline ? a.deadline < b.deadline : a.seq < b.seq;
            });

            fired.clear();
            std::size_t cnt = ntt_timer_heap_svc(&heap, now);

            ASSERT_EQ(cnt, expired.size()) << "step " << step;

            std::vector<int> expected;
            for (const Ref& r : expired) {
                expected.push_back(r.id);
            }
            ASSERT_EQ(fired, expected) << "step " << step;

            for (const Ref& r : expired) {
                live.erase(
                    std::find_if(live.begin(), live.end(), [&r](const auto& p) {
                        return p.second == r.id;
                    }));
                ref.erase(
                    std::find_if(ref.begin(), ref.end(), [&r](const Ref& x) {
                        return x.id == r.id;
                    }));
            }
        }

        ASSERT_EQ(ntt_timer_heap_size(&heap), ref.size()) << "step " << step;

        std::uint64_t peeked = 0;
        if (ntt_timer_heap_peek(&heap, &peeked) == 1) {
            std::uint64_t min_deadline = UINT64_MAX;
            for (const Ref& r : ref) {
                min_deadline = std::min(min_deadline, r.deadline);
            }
            ASSERT_EQ(peeked, min_deadline) << "step " << step;
        }
    }
}
