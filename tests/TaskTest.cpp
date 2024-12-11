#include "ntt/ntt.hpp"

#include <gtest/gtest.h>

#include <future>

class TaskTest : public testing::Test {
};

TEST_F(TaskTest, RawTask)
{
    int v = 0;
    auto task = ntt_make_task(
        +[](void* payload) {
            **static_cast<int**>(payload) = 1234;
        },
        NULL);
    *static_cast<int**>(task) = &v;
    ntt_do_task(task);
    ntt_free_task(task);
    ASSERT_EQ(v, 1234);
}

TEST_F(TaskTest, Common) {
  std::promise<int> promise;
  auto future = promise.get_future();
  auto *task = ntt::make_task(
      [promise = std::move(promise)] mutable { promise.set_value(9); });
  ntt::do_task(task);
  ntt::free_task(task);
  ASSERT_EQ(future.get(), 9);
}

TEST_F(TaskTest, DoTask) {
  int v = 0;
  auto task = ntt::make_task([&] { v = 1; });
  ntt::do_task(task);
  ntt::free_task(task);
  EXPECT_EQ(v, 1);
}

template <class T> bool is_aligned(const void *ptr) noexcept {
  auto iptr = reinterpret_cast<std::uintptr_t>(ptr);
  return !(iptr % alignof(T));
}

TEST_F(TaskTest, CheckDefaultMallocAlignment) {
  struct alignas(16) A {
    int a;
  };
  for (std::size_t j = 0; j < 100'000; ++j) {
    for (std::size_t i = 16; i < 256; ++i) {
      ASSERT_TRUE(is_aligned<A>(malloc(i)));
    }
  }
}

TEST_F(TaskTest, Alignment) {
  struct alignas(8) A {
    std::uint64_t a;
  } a;
  // struct alignas(16) B {
  //   std::uint64_t a;
  // } b;

  // TODO: over-aligned task payload data
  // struct alignas(32) C {
  //   std::uint64_t a;
  // } c;

  auto *task_a = ntt::make_task([a] {});
  EXPECT_TRUE(is_aligned<A>(task_a));
  ntt::free_task(task_a);

  // auto *task_b = ntt::make_task([b] {});
  // EXPECT_TRUE(is_aligned<B>(task_a));
  // ntt::free_task(task_a);

  // TODO: over-aligned task payload data
  // auto *task_c = ntt::make_task([c] {});
  // EXPECT_TRUE(is_aligned<C>(task_a));
  // ntt::free_task(task_a);
}
