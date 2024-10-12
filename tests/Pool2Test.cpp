#include "ntt/ntt.hpp"

#include <boost/asio.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <chrono>
#include <gtest/gtest.h>

#include <future>
#include <thread>

TEST(Pool2, Run) {
  static constexpr std::size_t width = 4;
  static constexpr std::size_t cnt = 100'000'000;
  auto *pool = ntt_pool2_create(width);
  std::atomic<int> g_cnt;
  std::promise<void> promise;
  auto future = promise.get_future();
  for (std::size_t i = 0; i < cnt; ++i) {
    ntt::post(pool, [&g_cnt, &promise] {
      if (g_cnt.fetch_add(1) == cnt - 1) {
        promise.set_value();
      }
    });
  }
  future.wait();
  ntt_pool2_release(pool);
  std::cout << g_cnt << std::endl;
}

TEST(Pool2, Run2) {
  static constexpr std::size_t width = 4;
  static constexpr std::size_t mul = 80;
  static constexpr std::size_t cnt = mul * mul * mul * mul;
  auto *pool = ntt_pool2_create(width);
  std::atomic<int> g_cnt;
  std::promise<void> promise;
  auto future = promise.get_future();
  ntt::post(pool, [pool, &g_cnt, &promise] {
    for (std::size_t i = 0; i < mul; ++i) {
      ntt::post(pool, [pool, &g_cnt, &promise] {
        for (std::size_t i = 0; i < mul; ++i) {
          ntt::post(pool, [pool, &g_cnt, &promise] {
            for (std::size_t i = 0; i < mul; ++i) {
              ntt::post(pool, [pool, &g_cnt, &promise] {
                for (std::size_t i = 0; i < mul; ++i) {
                  ntt::post(pool, [pool, &g_cnt, &promise] {
                    if (g_cnt.fetch_add(1) == cnt - 1) {
                      promise.set_value();
                    }
                  });
                }
              });
            }
          });
        }
      });
    }
  });

  future.wait();
  ntt_pool2_release(pool);
  std::cout << g_cnt << std::endl;
}

TEST(Pool2, Run2Boost) {
  static constexpr std::size_t width = 4;
  static constexpr std::size_t mul = 80;
  static constexpr std::size_t cnt = mul * mul * mul * mul;
  boost::asio::io_context pool;
  auto work_guard = boost::asio::make_work_guard(pool);
  std::vector<std::jthread> threads;
  for (std::size_t i = 0; i < width; ++i) {
    threads.emplace_back([&pool] { pool.run(); });
  }
  std::atomic<int> g_cnt;
  std::promise<void> promise;
  auto future = promise.get_future();
  boost::asio::post(pool, [&pool, &g_cnt, &promise] {
    for (std::size_t i = 0; i < mul; ++i) {
      boost::asio::post(pool, [&pool, &g_cnt, &promise] {
        for (std::size_t i = 0; i < mul; ++i) {
          boost::asio::post(pool, [&pool, &g_cnt, &promise] {
            for (std::size_t i = 0; i < mul; ++i) {
              boost::asio::post(pool, [&pool, &g_cnt, &promise] {
                for (std::size_t i = 0; i < mul; ++i) {
                  boost::asio::post(pool, [&pool, &g_cnt, &promise] {
                    if (g_cnt.fetch_add(1) == cnt - 1) {
                      promise.set_value();
                    }
                  });
                }
              });
            }
          });
        }
      });
    }
  });

  future.wait();
  work_guard.reset();
  std::cout << g_cnt << std::endl;
}

TEST(Pool2, Run2BoostDispatch) {
  static constexpr std::size_t width = 4;
  static constexpr std::size_t mul = 80;
  static constexpr std::size_t cnt = mul * mul * mul * mul;
  boost::asio::io_context pool;
  auto work_guard = boost::asio::make_work_guard(pool);
  std::vector<std::jthread> threads;
  for (std::size_t i = 0; i < width; ++i) {
    threads.emplace_back([&pool] { pool.run(); });
  }
  std::atomic<int> g_cnt;
  std::promise<void> promise;
  auto future = promise.get_future();
  boost::asio::dispatch(pool, [&pool, &g_cnt, &promise] {
    for (std::size_t i = 0; i < mul; ++i) {
      boost::asio::dispatch(pool, [&pool, &g_cnt, &promise] {
        for (std::size_t i = 0; i < mul; ++i) {
          boost::asio::dispatch(pool, [&pool, &g_cnt, &promise] {
            for (std::size_t i = 0; i < mul; ++i) {
              boost::asio::dispatch(pool, [&pool, &g_cnt, &promise] {
                for (std::size_t i = 0; i < mul; ++i) {
                  boost::asio::dispatch(pool, [&pool, &g_cnt, &promise] {
                    if (g_cnt.fetch_add(1) == cnt - 1) {
                      promise.set_value();
                    }
                  });
                }
              });
            }
          });
        }
      });
    }
  });

  future.wait();
  work_guard.reset();
  std::cout << g_cnt << std::endl;
}

TEST(Pool2, Run3Ntt) {
  static constexpr std::size_t width = 4;
  static constexpr std::size_t mul = 16;
  static constexpr std::size_t cnt = mul * mul * mul * mul;
  auto *pool = ntt_pool2_create(width);
  std::atomic<int> g_cnt;
  std::promise<void> promise;
  auto future = promise.get_future();
  ntt::post(pool, [pool, &g_cnt, &promise] {
    std::this_thread::sleep_for(std::chrono::microseconds{10});
    for (std::size_t i = 0; i < mul; ++i) {
      ntt::post(pool, [pool, &g_cnt, &promise] {
        std::this_thread::sleep_for(std::chrono::microseconds{10});
        for (std::size_t i = 0; i < mul; ++i) {
          ntt::post(pool, [pool, &g_cnt, &promise] {
            std::this_thread::sleep_for(std::chrono::microseconds{10});
            for (std::size_t i = 0; i < mul; ++i) {
              ntt::post(pool, [pool, &g_cnt, &promise] {
                std::this_thread::sleep_for(std::chrono::microseconds{10});
                for (std::size_t i = 0; i < mul; ++i) {
                  ntt::post(pool, [pool, &g_cnt, &promise] {
                    std::this_thread::sleep_for(std::chrono::microseconds{10});
                    if (g_cnt.fetch_add(1) == cnt - 1) {
                      promise.set_value();
                    }
                  });
                }
              });
            }
          });
        }
      });
    }
  });
  future.wait();
  ntt_pool2_release(pool);
  std::cout << g_cnt << std::endl;
}

TEST(Pool2, Run3BoostPost) {
  static constexpr std::size_t width = 4;
  static constexpr std::size_t mul = 16;
  static constexpr std::size_t cnt = mul * mul * mul * mul;
  boost::asio::io_context pool;
  auto work_guard = boost::asio::make_work_guard(pool);
  std::vector<std::jthread> threads;
  for (std::size_t i = 0; i < width; ++i) {
    threads.emplace_back([&pool] { pool.run(); });
  }
  std::atomic<int> g_cnt;
  std::promise<void> promise;
  auto future = promise.get_future();
  boost::asio::post(pool, [&pool, &g_cnt, &promise] {
    std::this_thread::sleep_for(std::chrono::microseconds{10});
    for (std::size_t i = 0; i < mul; ++i) {
      boost::asio::post(pool, [&pool, &g_cnt, &promise] {
        std::this_thread::sleep_for(std::chrono::microseconds{10});
        for (std::size_t i = 0; i < mul; ++i) {
          boost::asio::post(pool, [&pool, &g_cnt, &promise] {
            std::this_thread::sleep_for(std::chrono::microseconds{10});
            for (std::size_t i = 0; i < mul; ++i) {
              boost::asio::post(pool, [&pool, &g_cnt, &promise] {
                std::this_thread::sleep_for(std::chrono::microseconds{10});
                for (std::size_t i = 0; i < mul; ++i) {
                  boost::asio::post(pool, [&pool, &g_cnt, &promise] {
                    std::this_thread::sleep_for(std::chrono::microseconds{10});
                    if (g_cnt.fetch_add(1) == cnt - 1) {
                      promise.set_value();
                    }
                  });
                }
              });
            }
          });
        }
      });
    }
  });

  future.wait();
  work_guard.reset();
  std::cout << g_cnt << std::endl;
}

TEST(Pool2, Run3BoostDispatch) {
  static constexpr std::size_t width = 4;
  static constexpr std::size_t mul = 16;
  static constexpr std::size_t cnt = mul * mul * mul * mul;
  boost::asio::io_context pool;
  auto work_guard = boost::asio::make_work_guard(pool);
  std::vector<std::jthread> threads;
  for (std::size_t i = 0; i < width; ++i) {
    threads.emplace_back([&pool] { pool.run(); });
  }
  std::atomic<int> g_cnt;
  std::promise<void> promise;
  auto future = promise.get_future();
  boost::asio::dispatch(pool, [&pool, &g_cnt, &promise] {
    std::this_thread::sleep_for(std::chrono::microseconds{10});
    for (std::size_t i = 0; i < mul; ++i) {
      boost::asio::dispatch(pool, [&pool, &g_cnt, &promise] {
        std::this_thread::sleep_for(std::chrono::microseconds{10});
        for (std::size_t i = 0; i < mul; ++i) {
          boost::asio::dispatch(pool, [&pool, &g_cnt, &promise] {
            std::this_thread::sleep_for(std::chrono::microseconds{10});
            for (std::size_t i = 0; i < mul; ++i) {
              boost::asio::dispatch(pool, [&pool, &g_cnt, &promise] {
                std::this_thread::sleep_for(std::chrono::microseconds{10});
                for (std::size_t i = 0; i < mul; ++i) {
                  boost::asio::dispatch(pool, [&pool, &g_cnt, &promise] {
                    std::this_thread::sleep_for(std::chrono::microseconds{10});
                    if (g_cnt.fetch_add(1) == cnt - 1) {
                      promise.set_value();
                    }
                  });
                }
              });
            }
          });
        }
      });
    }
  });

  future.wait();
  work_guard.reset();
  std::cout << g_cnt << std::endl;
}
