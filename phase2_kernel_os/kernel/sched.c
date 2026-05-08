// sched.c：提供当前教学阶段最小可用的 PIT 时间片轮转调度入口
#include "process.h"
#include "sched.h"

// 当前仍是教学版最小调度器：只在 PIT 时间片边界做 READY 进程轮转，
// 不引入优先级、复杂运行队列或多核语义。

static int scheduler_enabled = 0;

void scheduler_init(void) {
    scheduler_enabled = 0;
}

void scheduler_start(void) {
    scheduler_enabled = 1;
}

unsigned int schedule(unsigned int current_esp) {
    if (scheduler_enabled == 0) {
        return current_esp;
    }

    return process_schedule_tick(current_esp);
}

int scheduler_is_enabled(void) {
    return scheduler_enabled;
}
