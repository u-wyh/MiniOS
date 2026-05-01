#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

// 进程状态：空闲、就绪、运行、僵尸
#define PROCESS_UNUSED 0
#define PROCESS_READY 1
#define PROCESS_RUNNING 2
#define PROCESS_ZOMBIE 3

// 最小 PCB：保存进程身份、父子关系、状态与用户态入口现场
struct process {
    int pid;
    int parent_pid;
    int state;
    uint32_t esp;
    uint32_t eip;
    int exit_status;

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
// 将当前进程标记为 ZOMBIE，并保存退出码
void process_exit(int status);
// 回收一个当前父进程名下的 ZOMBIE 子进程，成功返回 pid，失败返回 -1
int process_wait(void);
// 尝试回收指定 pid 的 ZOMBIE 子进程，返回 pid 或错误码
int process_waitpid(int pid);
// 状态码转可读字符串，供 ps 和文档对照使用
const char* process_state_name(int state);
// 返回当前进程 pid；无当前进程返回 0
int process_current_pid(void);
// 输出进程列表（PID / PPID / STATE）
void process_list(void);
// 返回已创建的进程数量
int process_count(void);

#endif
