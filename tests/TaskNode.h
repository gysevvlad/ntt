#pragma once

#include "ntt/task_queue.h"

#include <type_traits>
#include <utility>

namespace ntt {

namespace details {

template <class Functor> struct TaskNode : Functor {
  static void svc(void *arg) {
    auto task_node = static_cast<TaskNode *>(arg);
    (*task_node)();
    delete task_node;
  }
  static_assert(std::is_same_v<std::decay_t<Functor>, Functor>);
  template <class In> TaskNode(In &&func) : Functor(std::forward<In>(func)) {
    node.svc = svc;
    node.svc_arg = this;
  }
  ntt_task_node node;
};

} // namespace details

template <class Functor> ntt_task_node *make_task(Functor &&functor) {
  auto node = new details::TaskNode<std::decay_t<Functor>>(
      std::forward<Functor>(functor));
  return &node->node;
}

} // namespace ntt
