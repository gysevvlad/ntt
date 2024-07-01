#include <gtest/gtest.h>

#include "Event.h"

#include "ntt/work_loop.h"

#include <future>

TEST(ntt, work_loop) {
  static constexpr std::size_t g_task_cnt = 1'000'000;

  std::promise<void> p;

  ntt_work_loop_config config{
      .width = 4,
      .complete_arg = &p,
      .complete_cb =
          [](void *arg) {
            static_cast<std::promise<void> *>(arg)->set_value();
          },
  };
  auto *work_loop = ntt_work_loop_create_from_config(&config);

  std::atomic<std::size_t> cnt = 0;
  for (std::size_t i = 0; i < g_task_cnt; ++i) {
    ntt_work_loop_dispatch(
        work_loop,
        ntt::make_event([work_loop = ntt_work_loop_acquire(work_loop), &cnt] {
          cnt += 1;
          ntt_work_loop_release(work_loop);
        }));
  }

  ntt_work_loop_release(work_loop);

  p.get_future().wait();

  ASSERT_EQ(g_task_cnt, cnt);
}