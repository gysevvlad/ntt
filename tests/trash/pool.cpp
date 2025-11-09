#include "ntt/ntt.hpp"
#include "ntt/queue.h"
#include "ntt/task.h"

#include <chrono>
#include <gtest/gtest.h>

#include <future>
#include <ratio>
#include <thread>

TEST(NttPool, CommonTest) {
  std::atomic<std::size_t> cnt = 0;
  std::promise<void> done_promise;
  auto done_future = done_promise.get_future();
  static constexpr std::size_t task_cnt = 1'000'000;
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

TEST(NttPool, CommonQueue) {
  auto *pool = ntt_pool_create(4);
  auto queue = ntt_queue_create(pool);
  int cnt = 0;
  std::promise<void> done_promise;
  auto done_future = done_promise.get_future();
  static constexpr std::size_t task_cnt = 1'000'000;
  for (size_t i = 0; i < task_cnt; ++i) {
    ntt_queue_push(queue, ntt::make_task([&] {
                     cnt += 1;
                     if (cnt == task_cnt) {
                       done_promise.set_value();
                     }
                   }));
  }
  done_future.wait();
}

TEST(NttPool, PushQueue) {
  int cnt = 0;
  static constexpr std::size_t task_cnt = 1'000'000;
  struct Ctx {
    ntt_pool *pool = ntt_pool_create(3);
    ntt_queue *queue1 = ntt_queue_create(pool);
    ntt_queue *queue2 = ntt_queue_create(pool);
    ntt_queue *queue3 = ntt_queue_create(pool);
    std::array<std::chrono::high_resolution_clock::time_point, task_cnt>
        start{};
    std::array<std::chrono::high_resolution_clock::time_point, task_cnt> stop{};
    std::array<std::chrono::high_resolution_clock::duration, task_cnt> done{};
    std::promise<void> done_promise;
    std::future<void> done_future = done_promise.get_future();
  };
  auto ctx_ptr = std::make_unique<Ctx>();
  auto &ctx = *ctx_ptr;
  for (std::size_t i = 0; i < task_cnt; ++i) {
    ntt_queue_push(ctx.queue1, ntt::make_task([&, i] {
                     ctx.start[i] = std::chrono::high_resolution_clock::now();
                     ntt_queue_push(
                         ctx.queue2, ntt::make_task([&] {
                           ntt_queue_push(
                               ctx.queue3, ntt::make_task([&] {
                                 ctx.stop[i] =
                                     std::chrono::high_resolution_clock::now();
                                 ctx.done[i] = ctx.stop[i] - ctx.start[i];
                                 if (i == task_cnt - 1) {
                                   ctx.done_promise.set_value();
                                 }
                               }));
                         }));
                   }));
    std::this_thread::sleep_for(std::chrono::microseconds(1));
  }
  ctx.done_future.wait();
  std::sort(ctx.done.begin(), ctx.done.end());
  std::cout << "min: " << ctx.done[0].count() << std::endl;
  std::cout << "80%: " << ctx.done[(task_cnt / 100.0) * 80.0].count()
            << std::endl;
  std::cout << "max: " << ctx.done[task_cnt - 1].count() << std::endl;
}
