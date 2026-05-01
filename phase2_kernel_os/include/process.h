#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

// 进程状态：就绪、运行、退出
#define PROCESS_READY 0
#define PROCESS_RUNNING 1
#define PROCESS_EXIT 2

// 最小 PCB：本任务要求字段 pid/state/esp/eip
struct process {
    int pid;
    int state;
    uint32_t esp;
    uint32_t eip;

    // 扩展字段：记录程序名与槽位占用，便于 ps 展示与管理
    const char* name;
    int used;
};

// 初始化进程表与 PID 分配器
void process_init(void);
// 按文件名创建进程：分配 PID、加载 ELF、初始化 PCB
struct process* process_create(const char* name);
// 运行指定进程（切到用户态入口）
void process_run(struct process* proc);
// 标记当前进程退出态
void process_mark_current_exit(void);
// 返回当前进程 pid；无当前进程返回 0
int process_current_pid(void);
// 输出进程列表（PID / STATE）
void process_list(void);
// 返回已创建的进程数量
int process_count(void);

#endif
