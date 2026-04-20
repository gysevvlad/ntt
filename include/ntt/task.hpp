#pragma once

#include "ntt/task.h"

#include <memory>
#include <type_traits>

namespace ntt {

using task = ntt_task_t;

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
        });
    new (task) F { std::forward<FunctorT>(functor) };
    return task;
}

} // namespace ntt
