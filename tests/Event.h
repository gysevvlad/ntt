#pragma once

#include "ntt/event_queue.h"

#include <type_traits>
#include <utility>

namespace ntt {

namespace details {

template <class Functor> struct Event : Functor {
  static void svc(void *arg) {
    auto event = static_cast<Event *>(arg);
    (*event)();
    delete event;
  }
  static_assert(std::is_same_v<std::decay_t<Functor>, Functor>);
  template <class In> Event(In &&func) : Functor(std::forward<In>(func)) {
    event.svc = svc;
    event.svc_arg = this;
  }
  ntt_event event;
};

} // namespace details

template <class Functor> ntt_event *make_event(Functor &&functor) {
  auto event =
      new details::Event<std::decay_t<Functor>>(std::forward<Functor>(functor));
  return &event->event;
}

} // namespace ntt
