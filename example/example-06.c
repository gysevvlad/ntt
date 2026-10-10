// Timers: arm a deadline, arm a wakeup, cancel the deadline on the tick.

#include <ntt/ntt.h>

#include <stdio.h>

#define NTT_MS (1000 * 1000)

/**
 * @brief The deadline. Runs only if nobody cancelled it in time.
 */
static void on_deadline(void* payload, ntt_task_action_t action)
{
    (void)payload;

    if (action != NTT_TASK_RUN) {
        return;
    }

    printf("deadline fired\n");
}

/**
 * @brief The wakeup. Its payload holds the tag of the deadline.
 */
static void on_tick(void* payload, ntt_task_action_t action)
{
    ntt_timer_tag_t deadline = *(ntt_timer_tag_t*)payload;

    if (action != NTT_TASK_RUN) {
        return;
    }

    printf("tick, cancelling the deadline\n");

    // Cancelled from the context of the queue the timer was armed in.
    ntt_self_cancel_timer(deadline);
}

static void app_start(void* payload, ntt_task_action_t action)
{
    (void)payload;

    if (action != NTT_TASK_RUN) {
        return;
    }

    ntt_timer_tag_t deadline = ntt_self_wakeup_after(
        500 * NTT_MS,
        ntt_task_create(on_deadline));

    // ntt_task_create hands back the payload of the task, so the tag goes
    // straight into it.
    ntt_task_t* tick = ntt_task_create(on_tick);
    *(ntt_timer_tag_t*)tick = deadline;

    ntt_self_wakeup_after(50 * NTT_MS, tick);

    printf("armed: deadline in 500 ms, tick in 50 ms\n");
}

int main()
{
    // The loop runs while a timer is armed and stops by itself once the last
    // one has fired or has been cancelled.
    ntt_loop_run(ntt_task_create(app_start), 2);

    printf("loop finished\n");

    return 0;
}
