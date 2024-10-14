#include "ntt/pool2.h"
#include "ntt/ntt.hpp"
#include "ntt/queue.h"

#include <boost/asio.hpp>
#include <boost/asio/any_io_executor.hpp>
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

TEST(Pool2, Queue1) {
  static constexpr std::size_t qs_count = 128;
  static constexpr std::size_t gs_task_count = 1024;
  static constexpr std::size_t task_count = qs_count * gs_task_count;
  auto *pool = ntt_pool2_create(4);
  std::vector<ntt_queue_t *> qs;
  qs.reserve(qs_count);
  for (std::size_t i = 0; i < qs_count; ++i) {
    qs.emplace_back(ntt_queue_create(pool));
  }
  std::atomic<int> g_cnt;
  std::vector<std::size_t> queues_task_count;
  queues_task_count.resize(qs_count, 0);
  std::promise<void> promise;
  auto future = promise.get_future();
  for (std::size_t i = 0; i < gs_task_count; ++i) {
    for (std::size_t j = 0; j < qs_count; ++j) {
      ntt::post(qs[j],
                [&promise, &g_cnt, counter = &queues_task_count[j]] mutable {
                  *counter += 1;
                  if (*counter == gs_task_count) {
                    if (g_cnt.fetch_add(1) == qs_count - 1) {
                      promise.set_value();
                    }
                  }
                });
    }
  }
  future.wait();
}

TEST(Pool2, QueueDispatch1) {
  static constexpr std::size_t width = 4;
  static constexpr std::size_t qs_count = 128;
  static constexpr std::size_t qs_task_count = 1024;
  boost::asio::io_context pool;
  auto work_guard = boost::asio::make_work_guard(pool);
  std::vector<std::jthread> threads;
  for (std::size_t i = 0; i < width; ++i) {
    threads.emplace_back([&pool] { pool.run(); });
    threads.back().detach();
  }
  std::vector<boost::asio::strand<boost::asio::any_io_executor>> qs;
  qs.reserve(qs_count);
  for (std::size_t i = 0; i < qs_count; ++i) {
    qs.emplace_back(boost::asio::make_strand(pool));
  }
  std::atomic<int> g_cnt;
  std::vector<std::size_t> queues_task_count;
  queues_task_count.resize(qs_count, 0);
  std::promise<void> promise;
  auto future = promise.get_future();
  for (std::size_t i = 0; i < qs_task_count; ++i) {
    for (std::size_t j = 0; j < qs_count; ++j) {
      boost::asio::post(
          qs[j], [&promise, &g_cnt, counter = &queues_task_count[j]] mutable {
            *counter += 1;
            if (*counter == qs_task_count) {
              if (g_cnt.fetch_add(1) == qs_count - 1) {
                promise.set_value();
              }
            }
          });
    }
  }
  future.wait();
  work_guard.reset();
}
