#include "./slab.h"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <cstdlib>
#include <numeric>
#include <random>
#include <vector>

namespace {

/**
 * @brief As wide as ntt_queue_t, so the stride is the same 192 bytes.
 */
struct Object {
    ntt_slab_head_t head;
    char pad[160];
};

constexpr std::uint32_t g_stride = ((sizeof(Object) + 63) / 64) * 64;

/**
 * @brief What the library does: the stride is read from the slab.
 */
inline void* at_loaded(const ntt_slab_t* self, std::uint32_t index)
{
    std::uint32_t block = ntt_slab_block_of(index);

    std::uintptr_t base = atomic_load_explicit(
        &self->blocks[block],
        std::memory_order_acquire);

    return (char*)base + ((index - ntt_slab_block_first(block)) * self->stride);
}

/**
 * @brief The same with the stride known at compile time.
 */
inline void* at_const(const ntt_slab_t* self, std::uint32_t index)
{
    std::uint32_t block = ntt_slab_block_of(index);

    std::uintptr_t base = atomic_load_explicit(
        &self->blocks[block],
        std::memory_order_acquire);

    return (char*)base + ((index - ntt_slab_block_first(block)) * g_stride);
}

/**
 * @brief One flat array: no blocks at all, for reference.
 */
inline void* at_flat(char* base, std::uint32_t index)
{
    return base + (index * g_stride);
}

std::vector<std::uint32_t> shuffled(std::uint32_t cnt)
{
    std::vector<std::uint32_t> order(cnt);
    std::iota(order.begin(), order.end(), 0u);
    std::shuffle(order.begin(), order.end(), std::mt19937 { 777 });
    return order;
}

} // namespace

#define NTT_SLAB_RESOLVE_BODY(expr)                                           \
    const std::uint32_t cnt = static_cast<std::uint32_t>(state.range(0));     \
                                                                              \
    ntt_slab_t slab;                                                          \
    ntt_slab_init(&slab, sizeof(Object));                                     \
    for (std::uint32_t i = 0; i < cnt; ++i) {                                 \
        ntt_slab_take(&slab);                                                 \
    }                                                                         \
                                                                              \
    char* flat = static_cast<char*>(std::malloc((std::size_t)cnt* g_stride)); \
    for (std::uint32_t i = 0; i < cnt; ++i) {                                 \
        new (flat + ((std::size_t)i * g_stride)) Object {};                    \
    }                                                                         \
                                                                              \
    std::vector<std::uint32_t> order = shuffled(cnt);                         \
                                                                              \
    /* A mask, not a modulo: dividing by a runtime value costs more than the  \
       whole measurement. Every size is a power of two. */                    \
    const std::uint32_t mask = cnt - 1;                                       \
                                                                              \
    std::uint32_t i  = 0;                                                     \
    std::uint64_t s  = 0;                                                     \
    for (auto _ : state) {                                                    \
        s += (expr);                                                          \
        i = (i + 1) & mask;                                                   \
    }                                                                         \
    benchmark::DoNotOptimize(s);                                              \
                                                                              \
    std::free(flat);                                                          \
    ntt_slab_deinit(&slab);                                                   \
    state.SetItemsProcessed(state.iterations())

// Reading the generation is what every caller does right after resolving, and
// it is the load that may miss, so it belongs in the measurement.
static void BM_SlabResolveLoadedStride(benchmark::State& state)
{
    NTT_SLAB_RESOLVE_BODY(
        ntt_slab_generation(at_loaded(&slab, order[i])));
}

static void BM_SlabResolveConstStride(benchmark::State& state)
{
    NTT_SLAB_RESOLVE_BODY(
        ntt_slab_generation(at_const(&slab, order[i])));
}

static void BM_SlabResolveFlat(benchmark::State& state)
{
    NTT_SLAB_RESOLVE_BODY(
        ntt_slab_generation(at_flat(flat, order[i])));
}

BENCHMARK(BM_SlabResolveLoadedStride)->RangeMultiplier(16)->Range(64, 262144);
BENCHMARK(BM_SlabResolveConstStride)->RangeMultiplier(16)->Range(64, 262144);
BENCHMARK(BM_SlabResolveFlat)->RangeMultiplier(16)->Range(64, 262144);

/**
 * @brief Taking a slot and giving it back, with the free list never empty.
 *
 * Args: objects the slab has handed out before the measurement.
 */
static void BM_SlabTakeGiveLib(benchmark::State& state)
{
    ntt_slab_t slab;
    ntt_slab_init(&slab, sizeof(Object));

    std::vector<std::uint32_t> indices;
    for (std::int64_t i = 0; i < state.range(0); ++i) {
        indices.push_back(ntt_slab_index(ntt_slab_take(&slab)));
    }
    for (std::uint32_t index : indices) {
        ntt_slab_give(&slab, index);
    }

    for (auto _ : state) {
        void* object = ntt_slab_take(&slab);
        benchmark::DoNotOptimize(object);
        ntt_slab_give(&slab, ntt_slab_index(object));
    }

    ntt_slab_deinit(&slab);
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_SlabTakeGiveLib)->Arg(1)->Arg(64)->Arg(4096);

/** @brief Two lock/unlock pairs: what a take and a give pay before any work. */
static void BM_SlabLockPairs(benchmark::State& state)
{
    ntt_slab_t slab;
    ntt_slab_init(&slab, sizeof(Object));

    for (auto _ : state) {
        pthread_spin_lock(&slab.lock);
        pthread_spin_unlock(&slab.lock);
        pthread_spin_lock(&slab.lock);
        pthread_spin_unlock(&slab.lock);
    }

    ntt_slab_deinit(&slab);
}

BENCHMARK(BM_SlabLockPairs);
