#include "shell.h"
#include "commands.h"

#include <iostream>
#include <sstream>
#include <sys/wait.h>

void Shell::run() {
    std::string input;

    // 主循环：持续读取输入并分发给命令模块处理。
    while (true) {
        // 非阻塞回收已结束的后台子进程，避免僵尸进程累积。
        while (waitpid(-1, nullptr, WNOHANG) > 0) {}

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

        // 优先处理后台命令，避免被其他执行分支误判。
        if (executeBackgroundCommand(tokens)) {
            continue;
        }

        // 优先处理“单管道 + 输出重定向”组合命令。
        if (executePipeRedirectCommand(tokens)) {
            continue;
        }

        // 先处理输入重定向命令，处理后直接进入下一轮提示符。
        if (executeInputRedirectCommand(tokens)) {
            continue;
        }

        // 再处理输出重定向命令，处理后直接进入下一轮提示符。
        if (executeRedirectCommand(tokens)) {
            continue;
        }

        // 先处理单管道命令，处理后直接进入下一轮提示符。
        if (executePipeCommand(tokens)) {
            continue;
        }

        // 调用命令模块处理内建命令，并根据 shouldExit 决定是否结束。
        bool shouldExit = false;
        const bool handled = executeBuiltinCommand(tokens, shouldExit);
        if (!handled) {
            // 非内建命令自动按系统外部程序执行。
            executeExternalCommand(tokens);
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
