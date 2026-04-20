#include "./combine.h"
#include "./queue.h"
#include "ntt/queue.h"
#include "ntt/task.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <numeric>
#include <thread>

static constexpr std::size_t g_queue_cnt   = 8;
static constexpr std::size_t g_wave_cnt    = 16;
static constexpr std::size_t g_wave_degree = 2;
static constexpr std::size_t g_thread_cnt  = 2;

void spawn(std::size_t wave, std::deque<ntt_queue_t>& queues, std::deque<std::atomic<std::size_t>>& cnts)
{
    if (wave < g_wave_cnt) {
        for (std::size_t i = 0; i < g_wave_degree; ++i) {
            std::size_t j = std::rand() % g_queue_cnt;
            ntt_queue_push(
                &queues[j],
                ntt::make_task([wave = wave + 1, &queues, j, &cnts] {
                    cnts[j].store(cnts[j].load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
                    spawn(wave, queues, cnts);
                }));
        }
    }
}

TEST(CombineTest, Common)
{
    std::srand(std::time({}));

    ntt_combine_t combine;
    ntt_combine_init(&combine);

    std::deque<ntt_queue_t> queues;
    queues.resize(g_queue_cnt);
    for (std::size_t i = 0; i < g_queue_cnt; ++i) {
        ntt_queue_init(
            &queues[i],
            &combine,
            reinterpret_cast<ntt_queue_wakeup_cb_t*>(ntt_combine_push),
            nullptr);
    }

    std::deque<std::atomic<std::size_t>> cnts;
    for (std::size_t i = 0; i < g_queue_cnt; ++i) {
        cnts.emplace_back(0);
    }

    ntt_queue_push(
        &queues[0],
        ntt::make_task([&queues, &cnts] {
            cnts[0] += 1;
            spawn(1, queues, cnts);
        }));

    ntt_combine_svc(&combine, SIZE_MAX, SIZE_MAX);
    std::cout << "Total count: " << std::accumulate(cnts.begin(), cnts.end(), 0) << std::endl;
    std::cout << cnts[0];
    for (std::size_t i = 1; i < cnts.size(); ++i) {
        std::cout << '\t' << cnts[i];
    }
    std::cout << std::endl;
}

TEST(CombineTest, Multithreaded)
{
    std::srand(std::time({}));

    ntt_combine_t combine;
    ntt_combine_init(&combine);

    std::deque<ntt_queue_t> queues;
    queues.resize(g_queue_cnt);
    for (std::size_t i = 0; i < g_queue_cnt; ++i) {
        ntt_queue_init(
            &queues[i],
            &combine,
            reinterpret_cast<ntt_queue_wakeup_cb_t*>(ntt_combine_push),
            nullptr);
    }

    std::deque<std::atomic<std::size_t>> cnts;
    for (std::size_t i = 0; i < g_queue_cnt; ++i) {
        cnts.emplace_back(0);
    }

    static constexpr std::size_t g_task_cnt = (1 << g_wave_cnt) - 1;

    std::cout << g_task_cnt << std::endl;

    std::vector<std::jthread> threads;
    threads.reserve(g_thread_cnt);
    for (std::size_t i = 0; i < g_thread_cnt; ++i) {
        threads.emplace_back([&] {
            while (std::accumulate(cnts.begin(), cnts.end(), 0) != g_task_cnt) {
                ntt_combine_svc(&combine, 128, 1024);
                std::atomic_thread_fence(std::memory_order_acq_rel);
            }
        });
    }

    ntt_queue_push(
        &queues[0],
        ntt::make_task([&queues, &cnts] {
            cnts[0] += 1;
            spawn(1, queues, cnts);
        }));

    threads.clear();

    std::cout << "Total count: " << std::accumulate(cnts.begin(), cnts.end(), 0) << std::endl;
    std::cout << cnts[0];
    for (std::size_t i = 1; i < cnts.size(); ++i) {
        std::cout << '\t' << cnts[i];
    }
    std::cout << std::endl;
}
