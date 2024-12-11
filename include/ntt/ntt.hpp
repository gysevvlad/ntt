#pragma once

#include "ntt/ntt.h"
#include "ntt/sockaddr.h"

#include <cassert>
#include <memory>
#include <ostream>
#include <type_traits>

namespace ntt {

using task = ntt_task_t;

using pool = ntt_pool_t;

using queue = ntt_queue_t;

template <class FunctorT>
void post(pool* pool, FunctorT&& functor)
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
    auto task = ntt_make_task(
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

inline void do_task(task* task) { ntt_do_task(task); }

inline void free_task(task* task) { ntt_free_task(task); }

inline std::string to_string(const ntt_sockaddr_t* self)
{
    assert(self != nullptr);
    std::string buffer;
    buffer.resize(ntt_sockaddr_formatted_size(self));
    ntt_sockaddr_format_to(
        self, ntt_char_span_from_len_and_ptr(buffer.length(), buffer.data()));
    return buffer;
}

} // namespace ntt

inline std::ostream& operator<<(std::ostream& ostream,
    const ntt_sockaddr_t* self)
{
    if (self) {
        return ostream << ntt::to_string(self);
    }
    return ostream << "null";
}