#include <future>
#include <gtest/gtest.h>

#include "ntt/thread_pool.h"

TEST(ntt, thread_pool) {
  static constexpr std::size_t g_width = 4;

  std::atomic<std::size_t> cnt{0};
  std::promise<void> p;
  std::array<std::atomic<bool>, g_width> configured{};

  ntt_thread_pool_config config{};
  config.width = 4;
  config.configure_arg = &configured;
  config.configure_cb = [](void *arg, unsigned i) {
    (*static_cast<std::array<std::atomic<bool>, g_width> *>(arg))[i] = true;
  };
  config.svc_arg = &cnt;
  config.svc_cb = [](void *arg) {
    static_cast<std::atomic<std::size_t> *>(arg)->fetch_add(1);
  };
  config.complete_cb =
      [](void *arg) { static_cast<std::promise<void> *>(arg)->set_value(); },
  config.complete_arg = &p;

  ntt_thread_pool *thread_pool = ntt_thread_pool_create_from_config(&config);

  ntt_thread_pool_release(thread_pool);

  p.get_future().wait();

  for (std::size_t i = 0; i < g_width; ++i) {
    ASSERT_TRUE(configured[i]);
  }
  ASSERT_EQ(cnt, g_width);
}
