#pragma once

#include "ntt/ntt.h"
#include "ntt/pool.h"
#include "ntt/sockaddr.h"

#include <cassert>
#include <condition_variable>
#include <future>
#include <memory>
#include <ostream>
#include <type_traits>

namespace ntt {

using task = ntt_task_t;

using queue = ntt_queue_t;

template <class FunctorT>
void post(ntt_pool_t* pool, FunctorT&& functor)
{
    using F = std::remove_cvref_t<FunctorT>;
    static_assert(sizeof(F) <= NTT_TASK_PAYLOAD_SIZE);
    auto task = ntt_pool_alloc_task(
        pool, +[](void* payload) {
            auto* f = static_cast<F*>(
                std::assume_aligned<NTT_TASK_PAYLOAD_ALIGN>(payload));
            (*f)();
            f->~F();
        });
    new (task) F { std::forward<FunctorT>(functor) };
    ntt_pool_post_task(pool, task);
}

template <class FunctorT>
void post(queue* queue, FunctorT&& functor)
{
    using F = std::remove_cvref_t<FunctorT>;
    static_assert(sizeof(F) <= NTT_TASK_PAYLOAD_SIZE);
    auto task = ntt_queue_alloc_task(
        queue, +[](void* payload) {
            auto* f = static_cast<F*>(
                std::assume_aligned<NTT_TASK_PAYLOAD_ALIGN>(payload));
            (*f)();
            f->~F();
        });
    new (task) F { std::forward<FunctorT>(functor) };
    ntt_queue_push(queue, task);
}

template <class FunctorT>
task* make_task(FunctorT&& functor)
{
    using F = std::remove_cvref_t<FunctorT>;
    static_assert(sizeof(F) <= NTT_TASK_PAYLOAD_SIZE);
    auto task = ntt_task_create(
        +[](void* payload) {
            auto* f = static_cast<F*>(
                std::assume_aligned<NTT_TASK_PAYLOAD_ALIGN>(payload));
            (*f)();
            f->~F();
        },
        NULL);
    new (task) F { std::forward<FunctorT>(functor) };
    return task;
}

inline void do_task(task* task) { ntt_task_svc(task); }

inline void free_task(task* task) { ntt_task_destroy(task); }

inline std::string to_string(const ntt_sockaddr_t* self)
{
    assert(self != nullptr);
    std::string buffer;
    buffer.resize(ntt_sockaddr_format_to(self, nullptr, 0));
    ntt_sockaddr_format_to(self, buffer.data(), buffer.size());
    return buffer;
}

struct context {
    explicit context(unsigned width)
        : m_pool {
            ntt_pool_with_stopped_cb(
                width,
                +[](void* ctx) { static_cast<std::promise<void>*>(ctx)->set_value(); },
                &m_p)
        }
    {
    }

    ~context()
    {
        ntt_pool_release(m_pool);
        m_f.wait();
    }

    ntt_pool_t* as_pool() { return m_pool; }

private:
    std::promise<void> m_p;
    std::future<void> m_f = m_p.get_future();
    ntt_pool_t* m_pool;
};

} // namespace ntt

inline std::ostream& operator<<(std::ostream& ostream,
    const ntt_sockaddr_t* self)
{
    if (self) {
        return ostream << ntt::to_string(self);
    }
    return ostream << "null";
}