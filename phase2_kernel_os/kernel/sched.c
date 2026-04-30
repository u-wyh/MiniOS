#include "sched.h"
#include "task.h"

// 记录当前正在运行的任务编号，最小实现只在 0 和 1 之间轮转
static int current_task = 0;

// 初始化调度器：启动前默认先从任务 A 开始
void scheduler_init(void) {
    current_task = 0;
    task_mark_scheduled(current_task);
}

// 启动首个任务，把控制流从内核主流程切入任务上下文
void scheduler_start(void) {
    task_start_first(current_task);
}

// 最小 round-robin：保存旧任务 ESP，切到另一个任务的 ESP
unsigned int schedule(unsigned int current_esp) {
    int old_task = current_task;
    int next_task = (current_task + 1) % task_count();

    task_set_esp(old_task, current_esp);
    current_task = next_task;
    task_mark_scheduled(current_task);

    return task_get_esp(current_task);
}
