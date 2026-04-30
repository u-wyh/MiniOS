#ifndef TASK_H
#define TASK_H

// 最小任务控制块：当前阶段只保存栈指针
typedef struct {
    unsigned int esp;
} task_t;

// 初始化两个演示任务及其独立栈空间
void task_init(void);
// 手动切换到下一个任务，任务执行一步后会返回 Shell
void switch_task(void);

#endif
