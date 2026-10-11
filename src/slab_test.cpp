#include "./slab.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <set>
#include <vector>

namespace {

struct Object {
    ntt_slab_head_t head;
    std::uint64_t payload;
};

class SlabTest : public testing::Test {
protected:
    void SetUp() override { ntt_slab_init(&slab, sizeof(Object)); }
    void TearDown() override { ntt_slab_deinit(&slab); }

    Object* take() { return static_cast<Object*>(ntt_slab_take(&slab)); }

    Object* at(std::uint32_t index)
    {
        return static_cast<Object*>(ntt_slab_at(&slab, index));
    }

    ntt_slab_t slab {};
};

/**
 * @brief The block of an index, worked out by walking the blocks.
 */
std::uint32_t expected_block(std::uint32_t index)
{
    std::uint32_t block = 0;
    while (index >= ntt_slab_block_first(block + 1)) {
        block += 1;
    }
    return block;
}

} // namespace

TEST_F(SlabTest, StrideIsWholeCacheLines)
{
    ASSERT_EQ(slab.stride % 64, 0u);
    ASSERT_GE(slab.stride, sizeof(Object));
}

TEST_F(SlabTest, TakeHandsOutDistinctObjects)
{
    // Enough to cross several blocks, whatever the first block is.
    static constexpr std::uint32_t g_cnt = (NTT_FIRST_SLAB_BLOCK_LEN * 16) + 7;

    std::vector<Object*> objects;
    std::set<Object*> seen;

    for (std::uint32_t i = 0; i < g_cnt; ++i) {
        Object* object = take();

        ASSERT_NE(object, nullptr);
        ASSERT_TRUE(seen.insert(object).second) << "slot " << i << " handed out twice";
        ASSERT_EQ(ntt_slab_index(object), i);
        ASSERT_EQ(reinterpret_cast<std::uintptr_t>(object) % 64, 0u)
            << "slot " << i << " is not aligned";

        object->payload = i;
        objects.push_back(object);
    }

    // Every index resolves back to the object it was handed out as, which is
    // what the whole generation scheme rests on.
    for (std::uint32_t i = 0; i < g_cnt; ++i) {
        ASSERT_EQ(at(i), objects[i]) << "index " << i;
        ASSERT_EQ(at(i)->payload, i) << "index " << i;
    }
}

TEST_F(SlabTest, BlockBoundariesResolve)
{
    // The first index of every block and the one before it.
    std::vector<std::uint32_t> edges;
    for (std::uint32_t block = 0; block < 6; ++block) {
        std::uint32_t first = ntt_slab_block_first(block);
        if (first > 0) {
            edges.push_back(first - 1);
        }
        edges.push_back(first);
    }

    const std::uint32_t cnt = edges.back() + 1;

    std::vector<Object*> objects;
    for (std::uint32_t i = 0; i < cnt; ++i) {
        objects.push_back(take());
    }

    for (std::uint32_t index : edges) {
        ASSERT_EQ(at(index), objects[index]) << "index " << index;
        ASSERT_EQ(ntt_slab_block_of(index), expected_block(index)) << "index " << index;
    }
}

TEST_F(SlabTest, BlocksGrowByDoubling)
{
    for (std::uint32_t block = 0; block < 8; ++block) {
        ASSERT_EQ(ntt_slab_block_len(block), NTT_FIRST_SLAB_BLOCK_LEN << block);
        ASSERT_EQ(
            ntt_slab_block_first(block + 1),
            ntt_slab_block_first(block) + ntt_slab_block_len(block));
    }
}

TEST_F(SlabTest, BlocksCoverEveryIndex)
{
    // The largest index ever handed out stays below the sentinel.
    ASSERT_LT(NTT_SLAB_CAPACITY - 1, NTT_SLAB_NONE);

    // Walking the blocks in 64 bits covers the whole index range without
    // allocating anything, which is the only way to check the top of it.
    std::uint64_t first = 0;
    std::uint32_t block = 0;

    for (; first < NTT_SLAB_CAPACITY; ++block) {
        ASSERT_LT(block, NTT_SLAB_BLOCKS) << "the table is too small";
        ASSERT_EQ(ntt_slab_block_first(block), first) << "block " << block;

        const std::uint64_t count = block == NTT_SLAB_FULL_BLOCKS
            ? NTT_SLAB_LAST_BLOCK_LEN
            : static_cast<std::uint64_t>(NTT_FIRST_SLAB_BLOCK_LEN) << block;

        // Both ends of the block map back to it, the short last one included.
        ASSERT_EQ(ntt_slab_block_of(static_cast<std::uint32_t>(first)), block)
            << "block " << block;
        ASSERT_EQ(
            ntt_slab_block_of(static_cast<std::uint32_t>(first + count - 1)),
            block)
            << "block " << block;

        first += count;
    }

    // The blocks end exactly where the index does.
    ASSERT_EQ(first, NTT_SLAB_CAPACITY);
}

TEST_F(SlabTest, ReuseKeepsIndexAndBumpsGeneration)
{
    Object* first = take();

    std::uint32_t index      = ntt_slab_index(first);
    std::uint32_t generation = ntt_slab_generation(first);

    ntt_slab_give(&slab, index);

    Object* second = take();

    ASSERT_EQ(second, first);
    ASSERT_EQ(ntt_slab_index(second), index);
    ASSERT_NE(ntt_slab_generation(second), generation);
}

TEST_F(SlabTest, GenerationKeepsGrowingAcrossReuses)
{
    static constexpr int g_rounds = 100;

    std::set<std::uint32_t> generations;

    for (int i = 0; i < g_rounds; ++i) {
        Object* object = take();
        generations.insert(ntt_slab_generation(object));
        ntt_slab_give(&slab, ntt_slab_index(object));
    }

    // Never repeats, so a reference that saw an old one stays detectable.
    ASSERT_EQ(generations.size(), static_cast<std::size_t>(g_rounds));
    ASSERT_EQ(slab.size, 1u);
}

TEST_F(SlabTest, RetireBumpsWithoutGivingBack)
{
    Object* object = take();

    std::uint32_t before = ntt_slab_generation(object);
    std::uint32_t after  = ntt_slab_retire(object);

    ASSERT_EQ(after, before + 1);
    ASSERT_EQ(ntt_slab_generation(object), after);
    ASSERT_EQ(ntt_slab_index(object), 0u);
}

TEST_F(SlabTest, FreeListReusesSlots)
{
    static constexpr std::uint32_t g_cnt = NTT_FIRST_SLAB_BLOCK_LEN * 4;

    std::vector<std::uint32_t> indices;
    for (std::uint32_t i = 0; i < g_cnt; ++i) {
        indices.push_back(ntt_slab_index(take()));
    }

    for (std::uint32_t index : indices) {
        ntt_slab_give(&slab, index);
    }

    std::uint32_t watermark = slab.size;

    std::set<std::uint32_t> again;
    for (std::uint32_t i = 0; i < g_cnt; ++i) {
        again.insert(ntt_slab_index(take()));
    }

    // Nothing new was allocated, and every slot came back exactly once.
    ASSERT_EQ(slab.size, watermark);
    ASSERT_EQ(again.size(), g_cnt);
}
