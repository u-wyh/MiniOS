#ifndef TASK_H
#define TASK_H

// 最小任务控制块：当前阶段只保存栈指针
typedef struct {
    unsigned int esp;
} task_t;

// 初始化两个演示任务及其独立栈空间
void task_init(void);
// 启动首个任务，让 CPU 从内核主流程切入任务上下文
void task_start_first(int task_index);
// 读取指定任务当前保存的栈指针，供调度器切换使用
unsigned int task_get_esp(int task_index);
// 更新指定任务保存的栈指针，把中断现场写回 TCB
void task_set_esp(int task_index, unsigned int esp);
// 返回当前系统中的任务数量，供最小轮转调度器使用
int task_count(void);
// 标记任务被重新调度到 CPU，上层可用它控制任务一次只输出一个字符
void task_mark_scheduled(int task_index);
// 读取任务的运行代号，任务函数据此判断自己是否进入了新的时间片
unsigned int task_get_run_token(int task_index);

#endif
