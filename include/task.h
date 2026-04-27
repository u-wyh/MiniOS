#ifndef MINIOS_TASK_H
#define MINIOS_TASK_H

#include <string>
#include <vector>

#include <sys/types.h>

enum class TaskState {
    Ready,
    Running,
    Blocked,
    Done,
    Killed,
    Failed
};

struct TCB {
    int tid;
    pid_t hostPid;
    std::string command;
    TaskState state;
};

// 处理无前缀 Linux 风格任务命令（run/ps/kill/block/wake），返回是否已处理。
bool executeTaskCommand(const std::vector<std::string>& tokens);

// 创建受 MiniOS PCB 表管理的后台任务，成功返回 true 并写出 taskId/pid。
bool spawnManagedTask(const std::vector<std::string>& commandTokens, int& taskId, pid_t& pid);

// 在 Shell 回收子进程后同步更新任务状态。
void onProcessReaped(pid_t pid, int status);

// 调度器完成一次任务切换后，同步任务状态：prev Running->Ready，next Ready->Running。
void onTaskScheduled(int prevTid, int nextTid);

// 供同步原语调用的任务状态接口：按 tid 执行 block/wake。
bool blockTaskByTid(int tid);
bool wakeTaskByTid(int tid);

#endif
