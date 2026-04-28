#ifndef MINIOS_SEMAPHORE_H
#define MINIOS_SEMAPHORE_H

#include <queue>
#include <string>
#include <vector>

struct Semaphore {
    std::string name;
    int count;
    std::queue<int> waitQueue;
};

// 处理 sem 命令：create/wait/post/list。
bool executeSemaphoreCommand(const std::vector<std::string>& tokens);

#endif
