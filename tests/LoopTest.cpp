#include "ntt/loop.h"
#include "ntt/ntt.h"

#include <gtest/gtest.h>

TEST(NttLoopTest, Common)
{
    bool started = false;
    ntt_loop_svc(
        ntt_loop_cbs_t {
            .started = [](ntt_loop_t* loop, void* ctx) {
                (void)loop;
                *static_cast<bool*>(ctx) = true;
            },
        },
        &started, 4);

    EXPECT_TRUE(started);
}
