#include "./task_list.h"
#include "ntt/task.h"
#include "ntt/task.hpp"

#include <gtest/gtest.h>

TEST(TaskListTest, Common)
{
    static constexpr std::size_t g_cnt = 128;
    ntt_task_list_t l;
    ntt_task_list_init(&l);
    std::size_t j = 0;
    bool failed   = false;
    for (std::size_t i = 1; i <= g_cnt; ++i) {
        ntt_task_list_push(
            &l,
            ntt::make_task(
                [&j, &failed, i] {
                    if (j + 1 != i) {
                        failed = true;
                    }
                    j = i;
                }));
    }

    ntt_task_t* prev = NULL;
    ntt_task_t* task = ntt_task_list_front(&l);
    do {
        ntt_task_svc(task);
        prev = task;
        task = ntt_task_list_next(&l);
        ntt_task_destroy(prev);
    } while (task != NULL);

    ASSERT_FALSE(failed);
    ASSERT_EQ(j, g_cnt);
}
