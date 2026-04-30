#ifndef SCHED_H
#define SCHED_H

// 初始化最小轮转调度器，把当前任务指针定位到第一个演示任务
void scheduler_init(void);
// 启动第一个任务，让系统从内核主流程正式进入任务执行态
void scheduler_start(void);
// 根据当前中断现场选择下一个任务，并返回应恢复的目标 ESP
unsigned int schedule(unsigned int current_esp);
// 返回调度器是否已启用，供 PIT 在 shell 模式下仅累计 tick 而不切换任务
int scheduler_is_enabled(void);

#endif
