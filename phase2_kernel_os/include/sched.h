#ifndef SCHED_H
#define SCHED_H

// 调度器初始化：Task25 当前保留占位接口
void scheduler_init(void);
// 启动调度器：Task25 当前保留占位接口
void scheduler_start(void);
// 根据当前现场返回下一个应恢复的 ESP（当前直接返回原值）
unsigned int schedule(unsigned int current_esp);
// 返回调度器启用状态
int scheduler_is_enabled(void);

#endif
