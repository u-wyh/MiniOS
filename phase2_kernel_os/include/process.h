#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include "elf.h"

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
    // 记录用户栈资源：wait/waitpid 回收时释放该页
    uint32_t user_stack_va;
    uint32_t user_stack_pa;
    uint32_t user_stack_pages;
    // 记录用户代码/数据页资源：由 ELF 装载时填写，回收时逐页释放
    uint32_t user_page_count;
    uint32_t user_page_vaddr[ELF_LOAD_MAX_PAGES];
    uint32_t user_page_paddr[ELF_LOAD_MAX_PAGES];

    // 扩展字段：记录程序名与槽位占用，便于 ps 展示与管理
    const char* name;
    int used;
};

// 初始化进程表与 PID 分配器
void process_init(void);
// 按文件名创建并装载一个用户进程
struct process* process_create(const char* name);
// 把一份 ELF 镜像装载到指定进程中，成功后更新入口、用户栈和资源记录
int process_exec(struct process* proc, const unsigned char* elf_data, unsigned int elf_size);
// 按文件名装载或替换指定进程的用户镜像，供后续 exec 语义复用
int process_exec_file(struct process* proc, const char* name);
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
