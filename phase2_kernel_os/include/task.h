// task.h：保留旧 task 模型的兼容接口，内部已由 process 模型承载
#ifndef TASK_H
#define TASK_H

// 最小任务控制块：当前阶段只保存栈指针
typedef struct {
    unsigned int esp;
} task_t;

// Task25 之后该模块作为兼容层保留，初始化由 process 模型主导
void task_init(void);
// 兼容接口：当前为空实现
void task_start_first(int task_index);
// 兼容接口：当前返回占位值
unsigned int task_get_esp(int task_index);
// 兼容接口：当前不做实际保存
void task_set_esp(int task_index, unsigned int esp);
// 返回进程数量，供旧接口复用
int task_count(void);
// 兼容接口：当前为空实现
void task_mark_scheduled(int task_index);
// 兼容接口：当前返回占位值
unsigned int task_get_run_token(int task_index);

#endif
