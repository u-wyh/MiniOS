#include "scheduler.h"

#include <algorithm>
#include <iostream>

// 初始化调度器：默认 RR，且当前未选中任何任务。
Scheduler::Scheduler() : policy(SchedulerPolicy::RoundRobin), currentTid(-1) {}

void Scheduler::addTask(int tid) {
    // 仅接收有效 tid，避免非法任务号污染队列。
    if (tid <= 0) {
        return;
    }

    // 避免重复入队，保证 ready queue 中 tid 唯一。
    if (std::find(readyQueue.begin(), readyQueue.end(), tid) != readyQueue.end()) {
        return;
    }
    readyQueue.push_back(tid);
}

void Scheduler::removeTask(int tid) {
    // 仅处理有效 tid。
    if (tid <= 0) {
        return;
    }

    // 从 ready queue 中移除该任务；若它是 current 也同步清空。
    readyQueue.erase(std::remove(readyQueue.begin(), readyQueue.end(), tid), readyQueue.end());
    if (currentTid == tid) {
        currentTid = -1;
    }
}

void Scheduler::tick() {
    // 就绪队列为空时无法调度，保持 current 不变并给出清晰提示。
    if (readyQueue.empty()) {
        std::cout << "No runnable tasks\n";
        return;
    }

    // RR：先把上一轮 current 放回队尾，再取队首作为本轮 current。
    if (currentTid > 0) {
        readyQueue.push_back(currentTid);
    }

    currentTid = readyQueue.front();
    readyQueue.pop_front();
    std::cout << "Scheduled task: " << currentTid << '\n';
}

void Scheduler::status() const {
    // 本轮只支持 RR，因此状态输出固定为 RR。
    std::cout << "Policy: RR\n";
    if (currentTid > 0) {
        std::cout << "Current: " << currentTid << '\n';
    } else {
        std::cout << "Current: None\n";
    }

    std::cout << "Ready Queue:";
    if (readyQueue.empty()) {
        std::cout << " (empty)\n";
        return;
    }

    std::cout << ' ';
    for (std::size_t i = 0; i < readyQueue.size(); ++i) {
        if (i > 0) {
            std::cout << ' ';
        }
        std::cout << readyQueue[i];
    }
    std::cout << '\n';
}

void Scheduler::setPolicy(const std::string& policyName) {
    // 按任务要求，本轮只支持 rr 策略。
    if (policyName == "rr") {
        policy = SchedulerPolicy::RoundRobin;
        std::cout << "Scheduler policy set to RR\n";
        return;
    }
    std::cout << "Unsupported scheduler policy\n";
}

Scheduler& getScheduler() {
    // 进程内单例：TaskManager 与命令入口共享同一个调度器实例。
    static Scheduler scheduler;
    return scheduler;
}

bool executeSchedulerCommand(const std::vector<std::string>& tokens) {
    // 仅处理 sched 前缀，其余输入交回上层命令分发。
    if (tokens.empty() || tokens[0] != "sched") {
        return false;
    }

    Scheduler& scheduler = getScheduler();
    if (tokens.size() < 2) {
        std::cout << "Usage: sched <status|tick|policy>\n";
        return true;
    }

    const std::string& action = tokens[1];
    if (action == "status") {
        if (tokens.size() != 2) {
            std::cout << "Usage: sched status\n";
            return true;
        }
        scheduler.status();
        return true;
    }

    if (action == "tick") {
        if (tokens.size() != 2) {
            std::cout << "Usage: sched tick\n";
            return true;
        }
        scheduler.tick();
        return true;
    }

    if (action == "policy") {
        if (tokens.size() != 3) {
            std::cout << "Usage: sched policy rr\n";
            return true;
        }
        scheduler.setPolicy(tokens[2]);
        return true;
    }

    std::cout << "Usage: sched <status|tick|policy>\n";
    return true;
}
