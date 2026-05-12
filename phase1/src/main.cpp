#include "shell.h"

int main() {
    // 入口仅负责启动 Shell，保持职责单一。
    Shell shell;
    shell.run();
    return 0;
}
// g++ -std=c++17 -Wall -Wextra -Iinclude src/*.cpp -o minios