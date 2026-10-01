#include "./task.h"
#include "./timer_heap.h"

#include <benchmark/benchmark.h>

#include <boost/heap/pairing_heap.hpp>

#include <cstdint>
#include <deque>
#include <map>
#include <random>
#include <vector>

namespace {

void ntt_benchmark_noop(void*, ntt_task_action_t) { }

/**
 * @brief Pool of pre-created task nodes.
 *
 * Tasks are primed before the measured loop and recycled through their free
 * callback, so that the cost of the allocator stays out of the measurement and
 * only the timer structure is compared.
 *
 * The pool is per thread, because with several threads every thread runs the
 * whole benchmark body and owns its own timer structure.
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

/**
 * @brief Cheapest possible task, so that the timer structure dominates.
 *
 * Taken from the pool, so no allocation happens once the pool is primed.
 */
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

/**
 * @brief Pre-create cnt task nodes, so the measured loop never allocates.
 */
void prime_task_pool(std::size_t cnt)
{
    static thread_local std::vector<ntt_task_node_t*> pool;

    g_task_pool = &pool;

    // The pool never shrinks, so one reserve is enough to keep push_back in
    // the release callback allocation free.
    pool.reserve(pool.capacity() + cnt + 1024);

    while (pool.size() < cnt) {
        pool.push_back(ntt_benchmark_task_new());
    }
}

/**
 * @brief The timer heap of this project.
 */
class NttTimers {
public:
    using Tag = ntt_timer_tag_t;

    explicit NttTimers(unsigned = 1) { ntt_timer_heap_init(&heap_); }
    ~NttTimers() { ntt_timer_heap_deinit(&heap_); }

    Tag arm(unsigned, std::uint64_t deadline, ntt_task_t* task)
    {
        return ntt_timer_heap_push(&heap_, deadline, task);
    }

    void cancel(Tag tag) { ntt_timer_heap_cancel(&heap_, tag); }

    std::size_t svc(std::uint64_t now) { return ntt_timer_heap_svc(&heap_, now); }

    bool peek(std::uint64_t* deadline) const
    {
        return ntt_timer_heap_peek(&heap_, deadline) == 1;
    }

private:
    mutable ntt_timer_heap_t heap_ {};
};

/**
 * @brief Baseline: a std::multimap keyed by deadline, cancelled by iterator.
 *
 * The straightforward implementation. Ordering of equal deadlines is stable
 * because multimap::insert appends at the upper bound of the equal range.
 */
class MultimapTimers {
public:
    using Map = std::multimap<std::uint64_t, ntt_task_t*>;
    using Tag = Map::iterator;

    explicit MultimapTimers(unsigned = 1) { }

    ~MultimapTimers()
    {
        for (auto& entry : map_) {
            ntt_task_destroy(entry.second);
        }
    }

    Tag arm(unsigned, std::uint64_t deadline, ntt_task_t* task)
    {
        return map_.insert(Map::value_type { deadline, task });
    }

    void cancel(Tag tag)
    {
        ntt_task_destroy(tag->second);
        map_.erase(tag);
    }

    std::size_t svc(std::uint64_t now)
    {
        std::size_t cnt = 0;

        while (!map_.empty() && map_.begin()->first <= now) {
            ntt_task_t* task = map_.begin()->second;
            map_.erase(map_.begin());
            ntt_task_svc(task);
            ntt_task_destroy(task);
            cnt += 1;
        }

        return cnt;
    }

    bool peek(std::uint64_t* deadline) const
    {
        if (map_.empty()) {
            return false;
        }
        *deadline = map_.begin()->first;
        return true;
    }

private:
    Map map_;
};

/**
 * @brief Baseline: boost::heap::pairing_heap with stable handles.
 *
 * A mutable heap, so a timer is cancelled through its handle. Ordering is
 * made stable with the same (deadline, seq) key that NttTimers uses.
 */
class BoostPairingTimers {
public:
    struct Entry {
        std::uint64_t deadline;
        std::uint64_t seq;
        ntt_task_t* task;
    };

    /** Returns true when lhs must fire later, so the top is the earliest. */
    struct Later {
        bool operator()(const Entry& lhs, const Entry& rhs) const
        {
            if (lhs.deadline != rhs.deadline) {
                return lhs.deadline > rhs.deadline;
            }
            return lhs.seq > rhs.seq;
        }
    };

    using Heap = boost::heap::pairing_heap<Entry, boost::heap::compare<Later>>;
    using Tag  = Heap::handle_type;

    explicit BoostPairingTimers(unsigned = 1) { }

    ~BoostPairingTimers()
    {
        while (!heap_.empty()) {
            ntt_task_destroy(heap_.top().task);
            heap_.pop();
        }
    }

    Tag arm(unsigned, std::uint64_t deadline, ntt_task_t* task)
    {
        return heap_.push(Entry { deadline, seq_++, task });
    }

    void cancel(Tag tag)
    {
        ntt_task_destroy((*tag).task);
        heap_.erase(tag);
    }

    std::size_t svc(std::uint64_t now)
    {
        std::size_t cnt = 0;

        while (!heap_.empty() && heap_.top().deadline <= now) {
            ntt_task_t* task = heap_.top().task;
            heap_.pop();
            ntt_task_svc(task);
            ntt_task_destroy(task);
            cnt += 1;
        }

        return cnt;
    }

    bool peek(std::uint64_t* deadline) const
    {
        if (heap_.empty()) {
            return false;
        }
        *deadline = heap_.top().deadline;
        return true;
    }

private:
    Heap heap_;
    std::uint64_t seq_ = 0;
};

/**
 * @brief Variant of the timer heap with lazy cancellation.
 *
 * Cancelling does not touch the heap at all: the slot is released and its
 * version is bumped, which turns the heap entry into a tombstone. Tombstones
 * are recognised when they reach the top and are dropped there.
 *
 * Because of that the entry carries everything needed for ordering and for
 * the liveness check, and sifting never writes to the slab:
 *
 *   entry { deadline, seq, index, version }   24 bytes
 *   slot  { task, version }                   16 bytes
 *
 * The version must be in the entry, since the slot is reused by a later timer
 * and its version moves on. The sequence number must be there too: reading it
 * from a reused slot would silently change the sort key of a tombstone and
 * could leave a live entry stuck below it.
 *
 * Tombstones are reclaimed in bulk once they take over half of the heap, by
 * filtering them out and rebuilding the heap in O(n), which is cheaper per
 * tombstone than popping them one at a time.
 */
class LazyHeapTimers {
public:
    using Tag = ntt_timer_tag_t;

    explicit LazyHeapTimers(unsigned = 1) { }

    ~LazyHeapTimers()
    {
        for (const Elem& elem : heap_) {
            if (live(elem)) {
                ntt_task_destroy(slots_[elem.index].task);
            }
        }
    }

    Tag arm(unsigned, std::uint64_t deadline, ntt_task_t* task)
    {
        std::uint32_t index = alloc_slot();

        slots_[index].task = task;

        Elem elem;
        elem.deadline = deadline;
        elem.seq      = seq_++;
        elem.index    = index;
        elem.version  = slots_[index].version;

        heap_.push_back(elem);
        sift_up(heap_.size() - 1);

        Tag tag;
        tag.index   = index;
        tag.version = elem.version;
        return tag;
    }

    void cancel(Tag tag)
    {
        Slot& slot = slots_[tag.index];

        if (slot.version != tag.version) {
            return;
        }

        ntt_task_destroy(slot.task);
        free_slot(tag.index);
        tombstones_ += 1;

        compact_if_needed();
    }

    std::size_t svc(std::uint64_t now)
    {
        std::size_t cnt = 0;

        while (!heap_.empty() && heap_.front().deadline <= now) {
            Elem elem = heap_.front();
            pop_root();

            if (!live(elem)) {
                tombstones_ -= 1;
                continue;
            }

            ntt_task_t* task = slots_[elem.index].task;
            free_slot(elem.index);
            ntt_task_svc(task);
            ntt_task_destroy(task);
            cnt += 1;
        }

        return cnt;
    }

    bool peek(std::uint64_t* deadline)
    {
        drop_dead_root();

        if (heap_.empty()) {
            return false;
        }

        *deadline = heap_.front().deadline;
        return true;
    }

private:
    struct Elem {
        std::uint64_t deadline;
        std::uint64_t seq;
        std::uint32_t index;
        std::uint32_t version;
    };

    struct Slot {
        ntt_task_t* task;
        std::uint32_t version;
        std::uint32_t next_free;
    };

    static constexpr std::uint32_t g_none = 0xffffffffu;

    bool live(const Elem& elem) const
    {
        return slots_[elem.index].version == elem.version;
    }

    static bool less(const Elem& lhs, const Elem& rhs)
    {
        if (lhs.deadline != rhs.deadline) {
            return lhs.deadline < rhs.deadline;
        }
        return lhs.seq < rhs.seq;
    }

    std::uint32_t alloc_slot()
    {
        if (free_head_ != g_none) {
            std::uint32_t index = free_head_;
            free_head_          = slots_[index].next_free;
            slots_[index].version += 1;
            return index;
        }

        slots_.push_back(Slot { nullptr, 1, g_none });
        return static_cast<std::uint32_t>(slots_.size() - 1);
    }

    void free_slot(std::uint32_t index)
    {
        slots_[index].task = nullptr;
        slots_[index].version += 1;
        slots_[index].next_free = free_head_;
        free_head_              = index;
    }

    void sift_up(std::size_t pos)
    {
        Elem elem = heap_[pos];

        while (pos > 0) {
            std::size_t parent = (pos - 1) / 2;
            if (!less(elem, heap_[parent])) {
                break;
            }
            heap_[pos] = heap_[parent];
            pos        = parent;
        }

        heap_[pos] = elem;
    }

    void sift_down(std::size_t pos)
    {
        Elem elem = heap_[pos];

        for (;;) {
            std::size_t left = (pos * 2) + 1;
            if (left >= heap_.size()) {
                break;
            }

            std::size_t child = left;
            if (left + 1 < heap_.size() && less(heap_[left + 1], heap_[left])) {
                child = left + 1;
            }

            if (!less(heap_[child], elem)) {
                break;
            }

            heap_[pos] = heap_[child];
            pos        = child;
        }

        heap_[pos] = elem;
    }

    void pop_root()
    {
        heap_.front() = heap_.back();
        heap_.pop_back();
        if (!heap_.empty()) {
            sift_down(0);
        }
    }

    void drop_dead_root()
    {
        while (!heap_.empty() && !live(heap_.front())) {
            pop_root();
            tombstones_ -= 1;
        }
    }

    void compact_if_needed()
    {
        if (tombstones_ * 2 <= heap_.size()) {
            return;
        }

        std::size_t out = 0;
        for (std::size_t i = 0; i < heap_.size(); ++i) {
            if (live(heap_[i])) {
                heap_[out++] = heap_[i];
            }
        }
        heap_.resize(out);
        tombstones_ = 0;

        // Floyd, O(n).
        if (heap_.size() > 1) {
            for (std::size_t i = heap_.size() / 2; i-- > 0;) {
                sift_down(i);
            }
        }
    }

    std::vector<Elem> heap_;
    std::vector<Slot> slots_;
    std::uint32_t free_head_ = g_none;
    std::uint64_t seq_       = 0;
    std::size_t tombstones_  = 0;
};

/**
 * @brief Baseline for monotonically growing deadlines: an intrusive FIFO list.
 *
 * A request timeout is usually a constant, so deadlines are handed out in
 * increasing order. Then the earliest timer is always at the head and a new
 * one always belongs at the tail, which makes every operation O(1).
 *
 * Entries are recycled through a free list, so that arming does not allocate,
 * exactly like the slab of the timer heap.
 *
 * With several timeout classes there is one list per class and the earliest
 * deadline is the minimum over their heads, which costs a scan of the classes.
 *
 * NB! Within one svc call timers fire in class order, not in deadline order.
 * The ordering guarantee is per class only.
 *
 * @pre Deadlines of one class are passed in non decreasing order.
 */
class FifoDeadlineTimers {
public:
    struct Entry {
        Entry* prev;
        Entry* next;
        std::uint64_t deadline;
        ntt_task_t* task;
    };

    using Tag = Entry*;

    explicit FifoDeadlineTimers(unsigned classes = 1)
        : lists_(classes == 0 ? 1 : classes)
    {
        for (List& list : lists_) {
            list.sentinel.prev = &list.sentinel;
            list.sentinel.next = &list.sentinel;
        }
    }

    ~FifoDeadlineTimers()
    {
        for (List& list : lists_) {
            while (list.sentinel.next != &list.sentinel) {
                Entry* entry = list.sentinel.next;
                unlink(entry);
                ntt_task_destroy(entry->task);
                delete entry;
            }
        }

        while (free_list_ != nullptr) {
            Entry* entry = free_list_;
            free_list_   = entry->next;
            delete entry;
        }
    }

    Tag arm(unsigned cls, std::uint64_t deadline, ntt_task_t* task)
    {
        Entry& sentinel_ = lists_[cls].sentinel;

        Entry* entry = free_list_;
        if (entry != nullptr) {
            free_list_ = entry->next;
        } else {
            entry = new Entry {};
        }

        entry->deadline = deadline;
        entry->task     = task;

        entry->prev          = sentinel_.prev;
        entry->next          = &sentinel_;
        sentinel_.prev->next = entry;
        sentinel_.prev       = entry;

        return entry;
    }

    void cancel(Tag tag)
    {
        unlink(tag);
        ntt_task_destroy(tag->task);
        release(tag);
    }

    std::size_t svc(std::uint64_t now)
    {
        std::size_t cnt = 0;

        for (List& list : lists_) {
            Entry& sentinel_ = list.sentinel;
            while (sentinel_.next != &sentinel_ && sentinel_.next->deadline <= now) {
                Entry* entry     = sentinel_.next;
                ntt_task_t* task = entry->task;
                unlink(entry);
                release(entry);
                ntt_task_svc(task);
                ntt_task_destroy(task);
                cnt += 1;
            }
        }

        return cnt;
    }

    bool peek(std::uint64_t* deadline) const
    {
        bool found = false;

        for (const List& list : lists_) {
            const Entry& sentinel_ = list.sentinel;
            if (sentinel_.next == &sentinel_) {
                continue;
            }
            if (!found || sentinel_.next->deadline < *deadline) {
                *deadline = sentinel_.next->deadline;
                found     = true;
            }
        }

        return found;
    }

private:
    static void unlink(Entry* entry)
    {
        entry->prev->next = entry->next;
        entry->next->prev = entry->prev;
    }

    void release(Entry* entry)
    {
        entry->next = free_list_;
        free_list_  = entry;
    }

    struct List {
        Entry sentinel {};
    };

    // Fixed at construction, so the sentinels are never moved.
    std::vector<List> lists_;
    Entry* free_list_ = nullptr;
};

/**
 * @brief Arm cnt timers with far away deadlines and collect their tags.
 */
template <class Timers>
std::vector<typename Timers::Tag> fill(Timers& timers, std::int64_t cnt)
{
    std::mt19937 gen { 12345 };
    std::uniform_int_distribution<std::uint64_t> dist(1'000'000, 2'000'000);

    std::vector<typename Timers::Tag> tags;
    tags.reserve(static_cast<std::size_t>(cnt));

    for (std::int64_t i = 0; i < cnt; ++i) {
        tags.push_back(timers.arm(0, dist(gen), make_noop_task()));
    }

    return tags;
}

constexpr std::int64_t g_min_size = 1;
constexpr std::int64_t g_max_size = 1 << 16;

} // namespace

/**
 * @brief The deadline pattern: a timer is armed and then cancelled unfired.
 *
 * Keeps size() timers armed and measures one arm plus one cancel.
 */
template <class Timers>
static void BM_ArmAndCancel(benchmark::State& state)
{
    prime_task_pool(static_cast<std::size_t>(state.range(0)) + 16);

    Timers timers;
    std::vector<typename Timers::Tag> tags = fill(timers, state.range(0));

    std::mt19937 gen { 777 };
    std::uniform_int_distribution<std::uint64_t> dist(1'000'000, 2'000'000);

    std::size_t i = 0;
    for (auto _ : state) {
        timers.cancel(tags[i]);
        tags[i] = timers.arm(0, dist(gen), make_noop_task());
        i       = (i + 1) % tags.size();
    }

    state.SetItemsProcessed(state.iterations());
}

/**
 * @brief The wakeup pattern: a timer is armed and then fires.
 */
template <class Timers>
static void BM_ArmAndFire(benchmark::State& state)
{
    prime_task_pool(static_cast<std::size_t>(state.range(0)) + 16);

    Timers timers;
    std::vector<typename Timers::Tag> tags = fill(timers, state.range(0));
    benchmark::DoNotOptimize(tags.data());

    for (auto _ : state) {
        timers.arm(0, 0, make_noop_task());
        benchmark::DoNotOptimize(timers.svc(0));
    }

    state.SetItemsProcessed(state.iterations());
}

/**
 * @brief The honest wakeup pattern: a stream of timers that keep firing.
 *
 * Keeps about size() timers armed, spread uniformly over a window of time.
 * Every step arms one timer with a deadline somewhere inside that window, so
 * it interleaves with the timers already armed, then advances the clock by one
 * slot and runs whatever expired, which is one timer on average.
 *
 * Unlike BM_ArmAndFire this never arms a timer that is earlier than all the
 * others, which is the case a pairing heap handles in O(1) at both ends.
 */
template <class Timers>
static void BM_WakeupStream(benchmark::State& state)
{
    constexpr std::uint64_t g_step = 1024;

    const std::int64_t cnt       = state.range(0);
    const std::uint64_t window   = static_cast<std::uint64_t>(cnt) * g_step;

    prime_task_pool(static_cast<std::size_t>(cnt) + 16);

    Timers timers;

    std::mt19937 gen { 4242 };
    std::uniform_int_distribution<std::uint64_t> dist(0, window - 1);

    std::uint64_t now = 0;
    for (std::int64_t i = 0; i < cnt; ++i) {
        timers.arm(0, now + dist(gen), make_noop_task());
    }

    std::uint64_t fired = 0;
    for (auto _ : state) {
        timers.arm(0, now + dist(gen), make_noop_task());
        now += g_step;
        fired += timers.svc(now);
    }

    state.SetItemsProcessed(state.iterations());
    state.counters["fired_per_op"] = benchmark::Counter(
        static_cast<double>(fired) / static_cast<double>(state.iterations()));
}

/**
 * @brief The network request pattern: a constant timeout, mostly cancelled.
 *
 * Keeps about size() requests in flight. Every step arms a deadline at
 * now + timeout, which is later than every deadline already armed, then
 * retires the oldest one: with probability 95% it is cancelled just before it
 * would expire, as a request that got its response, and otherwise it is left
 * to fire as a timeout.
 */
template <class Timers>
static void BM_DeadlineStream(benchmark::State& state)
{
    constexpr std::uint64_t g_step       = 1024;
    constexpr unsigned g_cancel_percent  = 95;

    const std::int64_t cnt      = state.range(0);
    const std::uint64_t timeout = static_cast<std::uint64_t>(cnt) * g_step;

    prime_task_pool(static_cast<std::size_t>(cnt) + 16);

    Timers timers;

    std::mt19937 gen { 31337 };

    const auto will_cancel = [&gen] { return gen() % 100 < g_cancel_percent; };

    std::uint64_t now = 0;

    // In flight requests, oldest first.
    std::deque<typename Timers::Tag> in_flight;
    for (std::int64_t i = 1; i <= cnt; ++i) {
        std::uint64_t deadline = now + (static_cast<std::uint64_t>(i) * g_step);
        in_flight.push_back(timers.arm(0, deadline, make_noop_task()));
    }

    std::uint64_t cancelled = 0;
    std::uint64_t fired     = 0;

    for (auto _ : state) {
        // A constant timeout, so this deadline is later than every armed one.
        in_flight.push_back(timers.arm(0, now + timeout, make_noop_task()));

        // Retire the oldest request as answered. Its deadline is still in the
        // future, because the clock has not been advanced yet.
        if (will_cancel()) {
            timers.cancel(in_flight.front());
            in_flight.pop_front();
            cancelled += 1;
        }

        now += g_step;

        // Whatever reached its deadline times out. Those are the oldest ones,
        // so drop exactly as many entries from the front as actually ran.
        std::size_t ran = timers.svc(now);
        for (std::size_t i = 0; i < ran; ++i) {
            in_flight.pop_front();
        }
        fired += ran;
    }

    state.SetItemsProcessed(state.iterations());
    state.counters["cancelled_per_op"] = benchmark::Counter(
        static_cast<double>(cancelled) / static_cast<double>(state.iterations()));
    state.counters["fired_per_op"] = benchmark::Counter(
        static_cast<double>(fired) / static_cast<double>(state.iterations()));
    state.counters["in_flight"] = benchmark::Counter(
        static_cast<double>(in_flight.size()));
}

/**
 * @brief Arming only, with the structure growing from empty to size().
 */
template <class Timers>
static void BM_ArmOnly(benchmark::State& state)
{
    const std::int64_t cnt = state.range(0);

    prime_task_pool(static_cast<std::size_t>(cnt) + 16);

    for (auto _ : state) {
        Timers timers;

        std::mt19937 gen { 999 };
        std::uniform_int_distribution<std::uint64_t> dist(1, 1'000'000);

        for (std::int64_t i = 0; i < cnt; ++i) {
            benchmark::DoNotOptimize(timers.arm(0, dist(gen), make_noop_task()));
        }

        state.PauseTiming();
        timers.svc(UINT64_MAX);
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * cnt);
}

/**
 * @brief Draining size() expired timers in one svc call.
 */
template <class Timers>
static void BM_SvcBatch(benchmark::State& state)
{
    const std::int64_t cnt = state.range(0);

    prime_task_pool(static_cast<std::size_t>(cnt) + 16);

    Timers timers;

    for (auto _ : state) {
        state.PauseTiming();
        std::mt19937 gen { 555 };
        std::uniform_int_distribution<std::uint64_t> dist(1, 1000);
        for (std::int64_t i = 0; i < cnt; ++i) {
            timers.arm(0, dist(gen), make_noop_task());
        }
        state.ResumeTiming();

        benchmark::DoNotOptimize(timers.svc(UINT64_MAX));
    }

    state.SetItemsProcessed(state.iterations() * cnt);
}

/**
 * @brief Reading the earliest deadline.
 */
template <class Timers>
static void BM_Peek(benchmark::State& state)
{
    prime_task_pool(static_cast<std::size_t>(state.range(0)) + 16);

    Timers timers;
    std::vector<typename Timers::Tag> tags = fill(timers, state.range(0));
    benchmark::DoNotOptimize(tags.data());

    std::uint64_t deadline = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(timers.peek(&deadline));
    }
}

#define NTT_BENCHMARK_ALL(name)                                     \
    BENCHMARK_TEMPLATE(name, NttTimers)                             \
        ->RangeMultiplier(8)                                        \
        ->Range(g_min_size, g_max_size);                            \
    BENCHMARK_TEMPLATE(name, MultimapTimers)                        \
        ->RangeMultiplier(8)                                        \
        ->Range(g_min_size, g_max_size);                            \
    BENCHMARK_TEMPLATE(name, BoostPairingTimers)                    \
        ->RangeMultiplier(8)                                        \
        ->Range(g_min_size, g_max_size)


/**
 * @brief A market-like mix of request deadlines and periodic checks.
 *
 * Models two kinds of load at once:
 *
 *  - range(1) types of requests, say orders, balances or trades. Each type has
 *    its own timeout and keeps about range(0) requests in flight. These are
 *    deadline timers: 95% are cancelled when the response arrives and the rest
 *    time out.
 *
 *  - range(2) types of periodic checks, say healthchecks or a tick that
 *    triggers an action. Their period is shorter than any request timeout and
 *    they are expected to fire, so each one is re-armed as soon as it fires.
 *
 * Every type is its own timeout class, which is what breaks the single FIFO
 * list and makes the per class variant pay for the scan over the classes.
 *
 * The expected number of expirations is tracked by the harness, and any
 * disagreement with what the structure actually ran is reported in the
 * mismatch counter, which must stay at zero.
 */
template <class Timers>
static void BM_MarketMix(benchmark::State& state)
{
    constexpr std::uint64_t g_step      = 1024;
    constexpr unsigned g_cancel_percent = 95;

    const std::uint64_t inflight = static_cast<std::uint64_t>(state.range(0));
    const unsigned request_types = static_cast<unsigned>(state.range(1));
    const unsigned check_types   = static_cast<unsigned>(state.range(2));

    const std::uint64_t base = inflight * request_types * g_step;

    std::vector<std::uint64_t> request_timeout(request_types);
    for (unsigned i = 0; i < request_types; ++i) {
        request_timeout[i] = base + (static_cast<std::uint64_t>(i) * g_step);
    }

    std::vector<std::uint64_t> check_period(check_types);
    for (unsigned j = 0; j < check_types; ++j) {
        std::uint64_t period = base / (2 * (j + 1));
        check_period[j]      = period < g_step ? g_step : period;
    }

    prime_task_pool((inflight * request_types) + check_types + 16);

    Timers timers(request_types + check_types);

    std::mt19937 gen { 20250101 };
    const auto will_cancel = [&gen] { return gen() % 100 < g_cancel_percent; };

    struct Pending {
        typename Timers::Tag tag;
        std::uint64_t deadline;
    };

    std::uint64_t now = 0;

    std::vector<std::deque<Pending>> requests(request_types);
    for (unsigned i = 0; i < request_types; ++i) {
        for (std::uint64_t t = 1; t <= inflight; ++t) {
            std::uint64_t deadline = now + ((t * request_timeout[i]) / inflight);
            requests[i].push_back(
                Pending { timers.arm(i, deadline, make_noop_task()), deadline });
        }
    }

    std::vector<std::uint64_t> check_deadline(check_types);
    for (unsigned j = 0; j < check_types; ++j) {
        check_deadline[j] = now + check_period[j];
        timers.arm(request_types + j, check_deadline[j], make_noop_task());
    }

    std::uint64_t cancelled = 0;
    std::uint64_t fired     = 0;
    std::uint64_t mismatch  = 0;
    std::uint64_t it        = 0;

    for (auto _ : state) {
        const unsigned i = static_cast<unsigned>(it % request_types);
        it += 1;

        // A new request of type i, its deadline is the latest of that type.
        std::uint64_t deadline = now + request_timeout[i];
        requests[i].push_back(
            Pending { timers.arm(i, deadline, make_noop_task()), deadline });

        // Its oldest request usually gets a response before the deadline.
        if (!requests[i].empty() && will_cancel()) {
            timers.cancel(requests[i].front().tag);
            requests[i].pop_front();
            cancelled += 1;
        }

        now += g_step;

        std::size_t ran = timers.svc(now);

        // Requests that timed out, always the oldest ones of their type.
        std::size_t expected = 0;
        for (unsigned t = 0; t < request_types; ++t) {
            while (!requests[t].empty() && requests[t].front().deadline <= now) {
                requests[t].pop_front();
                expected += 1;
            }
        }

        // Checks fire and are armed again.
        for (unsigned j = 0; j < check_types; ++j) {
            while (check_deadline[j] <= now) {
                check_deadline[j] = now + check_period[j];
                timers.arm(request_types + j, check_deadline[j], make_noop_task());
                expected += 1;
            }
        }

        if (ran != expected) {
            mismatch += 1;
        }
        fired += ran;
    }

    state.SetItemsProcessed(state.iterations());
    state.counters["cancelled_per_op"] = benchmark::Counter(
        static_cast<double>(cancelled) / static_cast<double>(state.iterations()),
        benchmark::Counter::kAvgThreads);
    state.counters["fired_per_op"] = benchmark::Counter(
        static_cast<double>(fired) / static_cast<double>(state.iterations()),
        benchmark::Counter::kAvgThreads);
    state.counters["mismatch"] = benchmark::Counter(
        static_cast<double>(mismatch), benchmark::Counter::kAvgThreads);
}

// Every thread runs the whole body, so each one owns its timer structure.
// This measures how independent per queue structures scale, not sharing.
#define NTT_BENCHMARK_MIX(impl)            \
    BENCHMARK_TEMPLATE(BM_MarketMix, impl) \
        ->Args({ 1, 8, 3 })                \
        ->Args({ 8, 8, 3 })                \
        ->Args({ 64, 8, 3 })               \
        ->Args({ 512, 8, 3 })              \
        ->Args({ 64, 32, 8 })              \
        ->Threads(1)                       \
        ->Threads(2)                       \
        ->Threads(4)                       \
        ->Threads(8)

NTT_BENCHMARK_MIX(NttTimers);
NTT_BENCHMARK_MIX(LazyHeapTimers);
NTT_BENCHMARK_MIX(FifoDeadlineTimers);
NTT_BENCHMARK_MIX(MultimapTimers);
NTT_BENCHMARK_MIX(BoostPairingTimers);

BENCHMARK_TEMPLATE(BM_DeadlineStream, NttTimers)
    ->RangeMultiplier(8)
    ->Range(g_min_size, g_max_size);
BENCHMARK_TEMPLATE(BM_DeadlineStream, LazyHeapTimers)
    ->RangeMultiplier(8)
    ->Range(g_min_size, g_max_size);
BENCHMARK_TEMPLATE(BM_DeadlineStream, FifoDeadlineTimers)
    ->RangeMultiplier(8)
    ->Range(g_min_size, g_max_size);
BENCHMARK_TEMPLATE(BM_DeadlineStream, MultimapTimers)
    ->RangeMultiplier(8)
    ->Range(g_min_size, g_max_size);
BENCHMARK_TEMPLATE(BM_DeadlineStream, BoostPairingTimers)
    ->RangeMultiplier(8)
    ->Range(g_min_size, g_max_size);

NTT_BENCHMARK_ALL(BM_ArmAndCancel);
NTT_BENCHMARK_ALL(BM_ArmAndFire);
NTT_BENCHMARK_ALL(BM_WakeupStream);
NTT_BENCHMARK_ALL(BM_ArmOnly);
NTT_BENCHMARK_ALL(BM_SvcBatch);
NTT_BENCHMARK_ALL(BM_Peek);
