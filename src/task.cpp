#include "task.h"
#include "scheduler.h"

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <iomanip>
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

// 根据 waitpid 状态码更新任务状态，仅忽略已进入终态的任务。
void updateTaskStateByStatus(TCB& task, int status) {
    if (task.state == TaskState::Done || task.state == TaskState::Killed || task.state == TaskState::Failed) {
        return;
    }
    if (WIFEXITED(status)) {
        task.state = (WEXITSTATUS(status) == 0) ? TaskState::Done : TaskState::Failed;
        // 任务不可再运行，需从调度器就绪队列移除。
        getScheduler().removeTask(task.tid);
        return;
    }
    if (WIFSIGNALED(status)) {
        task.state = (WTERMSIG(status) == SIGTERM) ? TaskState::Killed : TaskState::Failed;
        // 信号终止同样要从调度视图中删除。
        getScheduler().removeTask(task.tid);
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
        std::cerr << "Unknown command\n";
        _exit(1);
    }

    TCB task{};
    task.tid = g_nextTid++;
    task.hostPid = pid;
    task.command = joinCommand(commandTokens);
    task.state = TaskState::Ready;
    g_tasks.push_back(task);
    taskId = task.tid;
    // 新任务创建成功后加入调度器 ready queue。
    getScheduler().addTask(task.tid);
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
            case TaskState::Blocked:
                return "Blocked";
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

    std::cout << std::left
              << std::setw(6) << "TID"
              << std::setw(8) << "PID"
              << std::setw(10) << "STATE"
              << "COMMAND\n";
    for (const auto& task : g_tasks) {
        std::cout << std::setw(6) << task.tid
                  << std::setw(8) << task.hostPid
                  << std::setw(10) << stateToString(task.state)
                  << task.command << '\n';
    }
}

// 按 tid 终止任务：仅终态任务不可重复 kill。
void killTaskById(const std::string& taskIdText) {
    refreshTaskStates();

    int tid = -1;
    try {
        tid = std::stoi(taskIdText);
    } catch (...) {
        std::cout << "Invalid argument\n";
        return;
    }

    auto it = std::find_if(g_tasks.begin(), g_tasks.end(), [tid](const TCB& task) {
        return task.tid == tid;
    });
    if (it == g_tasks.end()) {
        std::cout << "Task not found\n";
        return;
    }

    if (it->state == TaskState::Done || it->state == TaskState::Killed || it->state == TaskState::Failed) {
        std::cout << "Task is not running\n";
        return;
    }

    if (kill(it->hostPid, SIGTERM) != 0) {
        std::cout << "Failed to kill task\n";
        return;
    }
    it->state = TaskState::Killed;
    // kill 成功后立即从调度器移除，避免后续继续被 tick 选择。
    getScheduler().removeTask(it->tid);
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

TCB* findTaskByTid(int tid) {
    // 按 MiniOS tid 在线性任务表中查找对应 TCB。
    auto it = std::find_if(g_tasks.begin(), g_tasks.end(), [tid](const TCB& task) {
        return task.tid == tid;
    });
    if (it == g_tasks.end()) {
        return nullptr;
    }
    return &(*it);
}

bool doBlockTaskByTid(int tid) {
    // block 前先刷新一次状态，避免对已结束任务做阻塞操作。
    refreshTaskStates();

    TCB* task = findTaskByTid(tid);
    if (task == nullptr) {
        std::cout << "Task not found\n";
        return false;
    }

    // 仅 Ready/Running 任务允许进入 Blocked。
    if (task->state != TaskState::Ready && task->state != TaskState::Running) {
        std::cout << "Task cannot be blocked\n";
        return false;
    }

    const bool wasCurrent = getScheduler().isCurrent(tid);
    task->state = TaskState::Blocked;
    getScheduler().removeTask(tid);
    if (wasCurrent) {
        // 若阻塞的是当前任务，立即尝试切换到下一个可运行任务。
        getScheduler().tick();
    }
    return true;
}

bool doWakeTaskByTid(int tid) {
    // wake 前先刷新状态，确保状态判断基于最新任务视图。
    refreshTaskStates();

    TCB* task = findTaskByTid(tid);
    if (task == nullptr) {
        std::cout << "Task not found\n";
        return false;
    }

    // 只有 Blocked 任务可以被唤醒回 Ready。
    if (task->state != TaskState::Blocked) {
        std::cout << "Task is not blocked\n";
        return false;
    }

    task->state = TaskState::Ready;
    getScheduler().addTask(tid);
    return true;
}

}  // namespace

// 处理无前缀命令：run/ps/kill/block/wake，并在必要时回退到系统同名命令。
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

    if (command == "block") {
        if (tokens.size() != 2) {
            std::cout << "Usage: block <tid>\n";
            return true;
        }
        int tid = -1;
        try {
            tid = std::stoi(tokens[1]);
        } catch (...) {
            std::cout << "Invalid argument\n";
            return true;
        }
        doBlockTaskByTid(tid);
        return true;
    }

    if (command == "wake") {
        if (tokens.size() != 2) {
            std::cout << "Usage: wake <tid>\n";
            return true;
        }
        int tid = -1;
        try {
            tid = std::stoi(tokens[1]);
        } catch (...) {
            std::cout << "Invalid argument\n";
            return true;
        }
        doWakeTaskByTid(tid);
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

void onTaskScheduled(int prevTid, int nextTid) {
    // 调度器切换时同步 TCB 状态：prev 回到 Ready，next 进入 Running。
    if (prevTid > 0) {
        TCB* prevTask = findTaskByTid(prevTid);
        if (prevTask != nullptr && prevTask->state == TaskState::Running) {
            prevTask->state = TaskState::Ready;
        }
    }

    if (nextTid > 0) {
        TCB* nextTask = findTaskByTid(nextTid);
        if (nextTask != nullptr && nextTask->state == TaskState::Ready) {
            nextTask->state = TaskState::Running;
        }
    }
}

bool blockTaskByTid(int tid) {
    if (tid <= 0) {
        std::cout << "Invalid argument\n";
        return false;
    }
    return doBlockTaskByTid(tid);
}

bool wakeTaskByTid(int tid) {
    if (tid <= 0) {
        std::cout << "Invalid argument\n";
        return false;
    }
    return doWakeTaskByTid(tid);
}
