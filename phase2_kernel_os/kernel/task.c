#include "process.h"
#include "task.h"

// 兼容层：Task 接口在 Task25 之后转由 Process 模型承载
// 当前主流程已不再使用旧 task A/B 演示栈切换，这里保留最小实现避免接口断裂

void task_init(void) {
    // 进程模型由 process_init 负责初始化
}

void task_start_first(int task_index) {
    (void)task_index;
}

unsigned int task_get_esp(int task_index) {
    (void)task_index;
    return 0;
}

void task_set_esp(int task_index, unsigned int esp) {
    (void)task_index;
    (void)esp;
}

int task_count(void) {
    return process_count();
}

void task_mark_scheduled(int task_index) {
    (void)task_index;
}

unsigned int task_get_run_token(int task_index) {
    (void)task_index;
    return 0;
}
