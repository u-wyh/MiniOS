#ifndef MINIOS_TASK_H
#define MINIOS_TASK_H

#include <string>
#include <vector>

#include <sys/types.h>

enum class TaskState {
    Ready,
    Running,
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

// 处理无前缀 Linux 风格任务命令（run/ps/kill），返回是否已处理。
bool executeTaskCommand(const std::vector<std::string>& tokens);

// 创建受 MiniOS PCB 表管理的后台任务，成功返回 true 并写出 taskId/pid。
bool spawnManagedTask(const std::vector<std::string>& commandTokens, int& taskId, pid_t& pid);

// 在 Shell 回收子进程后同步更新任务状态。
void onProcessReaped(pid_t pid, int status);

#endif
