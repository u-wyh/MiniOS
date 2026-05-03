// sched.c：提供当前教学阶段的最小调度器占位实现
#include "sched.h"

// Task25 简化说明：当前仍为单核最小执行路径，调度器保持占位接口
// 真正的进程创建与运行切换入口在 process_create/process_run

static int scheduler_enabled = 0;

void scheduler_init(void) {
    scheduler_enabled = 0;
}

void scheduler_start(void) {
    scheduler_enabled = 0;
}

unsigned int schedule(unsigned int current_esp) {
    return current_esp;
}

int scheduler_is_enabled(void) {
    return scheduler_enabled;
}
