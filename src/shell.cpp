#include "shell.h"
#include "commands.h"

#include <iostream>
#include <sstream>

void Shell::run() {
    std::string input;

    // 主循环：持续读取输入并分发给命令模块处理。
    while (true) {
        std::cout << "MiniOS> ";
        std::getline(std::cin, input);

        // 输入流结束时直接退出，避免异常情况下死循环。
        if (!std::cin) {
            break;
        }

        // 先做最小解析：按空白切分，空输入直接进入下一轮。
        const std::vector<std::string> tokens = parseInput(input);
        if (tokens.empty()) {
            continue;
        }

        // 调用命令模块处理内建命令，并根据 shouldExit 决定是否结束。
        bool shouldExit = false;
        const bool handled = executeBuiltinCommand(tokens, shouldExit);
        if (!handled) {
            std::cout << "Unknown command\n";
        }
        if (shouldExit) {
            break;
        }
    }
}

std::vector<std::string> Shell::parseInput(const std::string& input) const {
    std::istringstream iss(input);
    std::vector<std::string> tokens;
    std::string token;

    // 逐个提取 token，保持解析逻辑简单清晰。
    while (iss >> token) {
        tokens.push_back(token);
    }

    return tokens;
}
