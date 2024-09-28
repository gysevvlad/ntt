#include "ntt/ntt.hpp"

#include <gtest/gtest.h>

#include <future>

TEST(NttPool, CommonTest) {
  std::atomic<std::size_t> cnt = 0;
  std::promise<void> done_promise;
  auto done_future = done_promise.get_future();
  static constexpr std::size_t task_cnt = 1'000;
  ntt_pool_t *pool = ntt_pool_create(4);
  for (std::size_t i = 0; i < task_cnt; ++i) {
    ntt_pool_push(pool, ntt::make_task([&] {
                    if (cnt.fetch_add(1) == task_cnt - 1) {
                      done_promise.set_value();
                    }
                  }));
  }
  done_future.wait();
  ntt_pool_release(pool);
}
