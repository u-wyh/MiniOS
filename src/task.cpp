#include "task.h"

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

namespace {

// MiniOS 内部任务表（PCB 列表）与自增任务编号。
std::vector<TCB> g_tasks;
int g_nextTid = 1;

// 把命令 token 拼成可读字符串，便于 task list 展示。
std::string joinCommand(const std::vector<std::string>& commandTokens) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < commandTokens.size(); ++i) {
        if (i > 0) {
            oss << ' ';
        }
        oss << commandTokens[i];
    }
    return oss.str();
}

// 根据 waitpid 状态码更新任务状态，仅处理仍在 Running 的任务。
void updateTaskStateByStatus(TCB& task, int status) {
    if (task.state != TaskState::Running) {
        return;
    }
    if (WIFEXITED(status)) {
        task.state = (WEXITSTATUS(status) == 0) ? TaskState::Done : TaskState::Failed;
        return;
    }
    if (WIFSIGNALED(status)) {
        task.state = (WTERMSIG(status) == SIGTERM) ? TaskState::Killed : TaskState::Failed;
    }
}

void refreshTaskStates() {
    // 轮询所有 Running 任务，非阻塞更新状态。
    for (auto& task : g_tasks) {
        if (task.state != TaskState::Running) {
            continue;
        }

        int status = 0;
        const pid_t result = waitpid(task.hostPid, &status, WNOHANG);
        if (result == task.hostPid) {
            updateTaskStateByStatus(task, status);
        }
    }
}

// 统一创建受管任务：fork + execvp，并把父进程侧信息写入 PCB 表。
bool spawnTaskInternal(const std::vector<std::string>& commandTokens, int& taskId, pid_t& pid) {
    pid = fork();
    if (pid < 0) {
        return false;
    }

    if (pid == 0) {
        std::vector<char*> argv;
        argv.reserve(commandTokens.size() + 1);
        for (const auto& token : commandTokens) {
            argv.push_back(const_cast<char*>(token.c_str()));
        }
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        std::cerr << "Command not found\n";
        _exit(1);
    }

    TCB task{};
    task.tid = g_nextTid++;
    task.hostPid = pid;
    task.command = joinCommand(commandTokens);
    task.state = TaskState::Running;
    g_tasks.push_back(task);
    taskId = task.tid;
    return true;
}

// 打印任务表前先刷新状态，确保 Running/Done 等信息尽量实时。
void listTasks() {
    const auto stateToString = [](TaskState state) -> const char* {
        switch (state) {
            case TaskState::Ready:
                return "Ready";
            case TaskState::Running:
                return "Running";
            case TaskState::Done:
                return "Done";
            case TaskState::Killed:
                return "Killed";
            case TaskState::Failed:
                return "Failed";
            default:
                return "Unknown";
        }
    };

    refreshTaskStates();

    std::cout << "TID   PID    STATE      COMMAND\n";
    for (const auto& task : g_tasks) {
        std::cout << task.tid << "    " << task.hostPid << "   " << stateToString(task.state)
                  << "    " << task.command << '\n';
    }
}

// 按 tid 终止任务：只允许结束 Running 任务。
void killTaskById(const std::string& taskIdText) {
    refreshTaskStates();

    int tid = -1;
    try {
        tid = std::stoi(taskIdText);
    } catch (...) {
        std::cout << "Invalid task id\n";
        return;
    }

    auto it = std::find_if(g_tasks.begin(), g_tasks.end(), [tid](const TCB& task) {
        return task.tid == tid;
    });
    if (it == g_tasks.end()) {
        std::cout << "Task not found\n";
        return;
    }

    if (it->state != TaskState::Running) {
        std::cout << "Task is not running\n";
        return;
    }

    if (kill(it->hostPid, SIGTERM) != 0) {
        std::cout << "Failed to kill task\n";
        return;
    }
    it->state = TaskState::Killed;
}

// 判断给定字符串是否对应当前 PCB 中存在的 tid。
bool hasTaskId(const std::string& taskIdText) {
    int tid = -1;
    try {
        tid = std::stoi(taskIdText);
    } catch (...) {
        return false;
    }

    const auto it = std::find_if(g_tasks.begin(), g_tasks.end(), [tid](const TCB& task) {
        return task.tid == tid;
    });
    return it != g_tasks.end();
}

}  // namespace

// 处理无前缀命令：run/ps/kill，并在必要时回退到系统同名命令。
bool executeTaskCommand(const std::vector<std::string>& tokens) {
    if (tokens.empty()) {
        return false;
    }

    const std::string& command = tokens[0];
    if (command == "run") {
        // 本轮按规范使用 run <command> & 创建后台任务。
        if (tokens.size() < 3 || tokens.back() != "&") {
            std::cout << "Usage: run <command> &\n";
            return true;
        }
        const std::vector<std::string> commandTokens(tokens.begin() + 1, tokens.end() - 1);
        int taskId = 0;
        pid_t pid = 0;
        if (!spawnTaskInternal(commandTokens, taskId, pid)) {
            std::cout << "Failed to spawn task\n";
            return true;
        }
        std::cout << "[started task] id=" << taskId << " pid=" << pid << '\n';
        return true;
    }

    if (command == "ps") {
        if (tokens.size() != 1) {
            // 带参数的 ps 继续走系统命令，保持 Linux 习惯兼容。
            return false;
        }
        listTasks();
        return true;
    }

    if (command == "kill") {
        if (tokens.size() != 2) {
            // 非 kill <id> 场景交给系统 kill 处理。
            return false;
        }
        if (!hasTaskId(tokens[1])) {
            // 找不到 task id 时，交给系统 kill（按 pid 等）处理。
            return false;
        }
        killTaskById(tokens[1]);
        return true;
    }

    return false;
}

void onProcessReaped(pid_t pid, int status) {
    // Shell 统一回收子进程后，任务表按 pid 同步状态。
    for (auto& task : g_tasks) {
        if (task.hostPid == pid) {
            updateTaskStateByStatus(task, status);
            break;
        }
    }
}

// 对外暴露的受管任务创建入口，供 cmd & 等路径复用同一 PCB 管理。
bool spawnManagedTask(const std::vector<std::string>& commandTokens, int& taskId, pid_t& pid) {
    if (commandTokens.empty()) {
        return false;
    }
    return spawnTaskInternal(commandTokens, taskId, pid);
}
