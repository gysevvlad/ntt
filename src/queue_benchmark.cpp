#include "./queue.h"
#include "./task.h"
#include "ntt/task.hpp"

#include <benchmark/benchmark.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

void ntt_benchmark_noop(void*, ntt_task_action_t) { }

/**
 * @brief Pool of pre-created task nodes, so the allocator stays out of the way.
 */
thread_local std::vector<ntt_task_node_t*>* g_task_pool = nullptr;

void ntt_benchmark_task_release(void* node)
{
    g_task_pool->push_back(static_cast<ntt_task_node_t*>(node));
}

ntt_task_node_t* ntt_benchmark_task_new()
{
    return ntt_task_create_inl(ntt_benchmark_noop, ntt_benchmark_task_release);
}

ntt_task_t* make_noop_task()
{
    ntt_task_node_t* node = nullptr;

    if (!g_task_pool->empty()) {
        node = g_task_pool->back();
        g_task_pool->pop_back();
    } else {
        node = ntt_benchmark_task_new();
    }

    ntt_node_init(&node->node);

    return &node->payload;
}

void prime_task_pool(std::size_t cnt)
{
    static thread_local std::vector<ntt_task_node_t*> pool;

    g_task_pool = &pool;
    pool.reserve(pool.capacity() + cnt + 1024);

    while (pool.size() < cnt) {
        pool.push_back(ntt_benchmark_task_new());
    }
}

/**
 * @brief Task that counts itself, for the cases that track consumption.
 */
void ntt_benchmark_count(void* payload, ntt_task_action_t action)
{
    if (action != NTT_TASK_RUN) {
        return;
    }

    *(*static_cast<std::int64_t**>(payload)) += 1;
}

ntt_task_t* make_counting_task(std::int64_t* counter)
{
    ntt_task_node_t* node = nullptr;

    if (!g_task_pool->empty()) {
        node = g_task_pool->back();
        g_task_pool->pop_back();
    } else {
        node = ntt_benchmark_task_new();
    }

    ntt_node_init(&node->node);
    node->task_cb = ntt_benchmark_count;

    *reinterpret_cast<std::int64_t**>(&node->payload) = counter;

    return &node->payload;
}

void ntt_benchmark_wakeup(void* ctx, ntt_queue_t*)
{
    *static_cast<unsigned*>(ctx) += 1;
}

/**
 * @brief A queue with no loop and no combine behind it.
 */
class Queue {
public:
    Queue()
    {
        ntt_queue_init(&queue_, &wakeups_, ntt_benchmark_wakeup, nullptr);
    }

    ntt_queue_t* get() { return &queue_; }

private:
    unsigned wakeups_ = 0;
    ntt_queue_t queue_ {};
};

} // namespace

/**
 * @brief One task at a time: push, then drain.
 *
 * The worst shape for the lock that ntt_queue_front takes, because it is taken
 * once per drain and here a drain is one task.
 */
static void BM_QueueSvcSingle(benchmark::State& state)
{
    prime_task_pool(16);

    Queue queue;

    for (auto _ : state) {
        ntt_queue_push_task(queue.get(), make_noop_task());
        benchmark::DoNotOptimize(ntt_queue_svc_with_limit(queue.get(), 1));
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_QueueSvcSingle);

/**
 * @brief Batches: push range(0) tasks, then drain them in one call.
 *
 * Shows the same lock amortized over the batch.
 */
static void BM_QueueSvcBatch(benchmark::State& state)
{
    const std::int64_t cnt = state.range(0);

    prime_task_pool(static_cast<std::size_t>(cnt) + 16);

    Queue queue;

    for (auto _ : state) {
        for (std::int64_t i = 0; i < cnt; ++i) {
            ntt_queue_push_task(queue.get(), make_noop_task());
        }
        benchmark::DoNotOptimize(
            ntt_queue_svc_with_limit(queue.get(), static_cast<std::size_t>(cnt)));
    }

    state.SetItemsProcessed(state.iterations() * cnt);
}
BENCHMARK(BM_QueueSvcBatch)->RangeMultiplier(8)->Range(1, 512);

/**
 * @brief A producer on another thread, a consumer draining one task at a time.
 *
 * This is the shape where the head of the list is read while somebody is
 * pushing, so it is both the contended case and the one that needs the
 * publication of the task payload to be synchronized.
 */
static void BM_QueueSvcCrossThread(benchmark::State& state)
{
    using clock_type = std::chrono::steady_clock;

    static constexpr std::int64_t g_tasks = 200000;

    for (auto _ : state) {
        prime_task_pool(1024);

        Queue queue;

        std::atomic<std::int64_t> pushed { 0 };
        std::atomic<bool> go { false };

        std::int64_t consumed = 0;

        std::thread producer { [&] {
            prime_task_pool(1024);

            while (!go.load(std::memory_order_acquire)) {
            }

            for (std::int64_t i = 0; i < g_tasks; ++i) {
                ntt_queue_push_task(queue.get(), make_counting_task(&consumed));
                pushed.fetch_add(1, std::memory_order_release);
            }
        } };

        auto start = clock_type::now();
        go.store(true, std::memory_order_release);

        // The tasks count themselves: svc_with_limit reports zero when it hits
        // the limit with tasks still queued, so its result cannot be used for
        // this.
        while (consumed < g_tasks) {
            if (pushed.load(std::memory_order_acquire) <= consumed) {
                continue;
            }

            benchmark::DoNotOptimize(ntt_queue_svc_with_limit(queue.get(), 1));
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
            clock_type::now() - start);

        producer.join();

        state.SetIterationTime(elapsed.count());
    }

    state.SetItemsProcessed(state.iterations() * g_tasks);
}
BENCHMARK(BM_QueueSvcCrossThread)->UseManualTime();
