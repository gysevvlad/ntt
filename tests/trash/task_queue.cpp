#include "ntt/ntt.hpp"

#include <gtest/gtest.h>
#include <thread>

TEST(NttTaskQueueTest, Common) {
  ntt_task_queue_t *task_queue = ntt_task_queue_create();

  int cnt = 0;
  {
    std::jthread thread([&] {
      int last;
      ntt::do_task(ntt_task_queue_pop_front_blocking(task_queue, &last));
      ntt::do_task(ntt_task_queue_pop_front_blocking(task_queue, &last));
      ntt::do_task(ntt_task_queue_pop_front_blocking(task_queue, &last));
      ntt::do_task(ntt_task_queue_pop_front_blocking(task_queue, &last));
      ntt::do_task(ntt_task_queue_pop_front_blocking(task_queue, &last));
    });

    int last;
    ntt_task_queue_push_back(task_queue, ntt::make_task([&] { cnt += 1; }),
                             &last);
    ntt_task_queue_push_back(task_queue, ntt::make_task([&] { cnt += 1; }),
                             &last);
    ntt_task_queue_push_back(task_queue, ntt::make_task([&] { cnt += 1; }),
                             &last);
    ntt_task_queue_push_back(task_queue, ntt::make_task([&] { cnt += 1; }),
                             &last);
    ntt_task_queue_push_back(task_queue, ntt::make_task([&] { cnt += 1; }),
                             &last);
  }
  ntt_task_queue_free(task_queue);
  EXPECT_EQ(cnt, 5);
}