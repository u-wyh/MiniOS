#include "shell.h"

int main() {
    // 入口仅负责启动 Shell，保持职责单一。
    Shell shell;
    shell.run();
    return 0;
}
// g++ -std=c++17 src/main.cpp src/shell.cpp src/commands.cpp src/task.cpp src/scheduler.cpp src/semaphore.cpp src/memory.cpp src/fs.cpp -Iinclude -o MiniOS
