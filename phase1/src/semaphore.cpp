#include "semaphore.h"

#include "scheduler.h"
#include "task.h"

#include <iomanip>
#include <iostream>
#include <unordered_map>

namespace {

std::unordered_map<std::string, Semaphore> g_semaphores;

void createSemaphore(const std::vector<std::string>& tokens) {
    if (tokens.size() != 4) {
        std::cout << "Usage: sem create <name> <count>\n";
        return;
    }

    const std::string& name = tokens[2];
    int count = 0;
    try {
        count = std::stoi(tokens[3]);
    } catch (...) {
        std::cout << "Invalid argument\n";
        return;
    }

    if (count < 0) {
        std::cout << "Invalid argument\n";
        return;
    }

    if (g_semaphores.find(name) != g_semaphores.end()) {
        std::cout << "Semaphore already exists\n";
        return;
    }

    Semaphore sem{name, count, {}};
    g_semaphores.emplace(name, sem);
}

int ensureCurrentTid() {
    Scheduler& scheduler = getScheduler();
    int tid = scheduler.getCurrentTid();
    if (tid > 0) {
        return tid;
    }

    // 没有 current 时尝试推进一次调度，选出一个可运行任务。
    scheduler.tick();
    return scheduler.getCurrentTid();
}

void waitSemaphore(const std::vector<std::string>& tokens) {
    if (tokens.size() != 3) {
        std::cout << "Usage: sem wait <name>\n";
        return;
    }

    auto it = g_semaphores.find(tokens[2]);
    if (it == g_semaphores.end()) {
        std::cout << "Semaphore not found\n";
        return;
    }

    Semaphore& sem = it->second;
    if (sem.count > 0) {
        sem.count--;
        return;
    }

    const int currentTid = ensureCurrentTid();
    if (currentTid <= 0) {
        std::cout << "No runnable tasks\n";
        return;
    }

    sem.waitQueue.push(currentTid);
    if (!blockTaskByTid(currentTid)) {
        sem.waitQueue.pop();
    }
}

void postSemaphore(const std::vector<std::string>& tokens) {
    if (tokens.size() != 3) {
        std::cout << "Usage: sem post <name>\n";
        return;
    }

    auto it = g_semaphores.find(tokens[2]);
    if (it == g_semaphores.end()) {
        std::cout << "Semaphore not found\n";
        return;
    }

    Semaphore& sem = it->second;
    sem.count++;
    if (sem.waitQueue.empty()) {
        return;
    }

    const int tid = sem.waitQueue.front();
    sem.waitQueue.pop();
    if (wakeTaskByTid(tid)) {
        // 资源直接交给被唤醒任务。
        sem.count--;
    }
}

void listSemaphore() {
    std::cout << std::left
              << std::setw(16) << "NAME"
              << std::setw(10) << "COUNT"
              << "WAITERS\n";
    for (const auto& [name, sem] : g_semaphores) {
        std::cout << std::setw(16) << name
                  << std::setw(10) << sem.count
                  << sem.waitQueue.size() << '\n';
    }
}

}  // namespace

bool executeSemaphoreCommand(const std::vector<std::string>& tokens) {
    if (tokens.empty() || tokens[0] != "sem") {
        return false;
    }

    if (tokens.size() < 2) {
        std::cout << "Usage: sem <create|wait|post|list>\n";
        return true;
    }

    const std::string& action = tokens[1];
    if (action == "create") {
        createSemaphore(tokens);
        return true;
    }
    if (action == "wait") {
        waitSemaphore(tokens);
        return true;
    }
    if (action == "post") {
        postSemaphore(tokens);
        return true;
    }
    if (action == "list") {
        if (tokens.size() != 2) {
            std::cout << "Usage: sem list\n";
            return true;
        }
        listSemaphore();
        return true;
    }

    std::cout << "Usage: sem <create|wait|post|list>\n";
    return true;
}
