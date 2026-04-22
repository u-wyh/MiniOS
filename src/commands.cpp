#include "commands.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>

bool executeBuiltinCommand(const std::vector<std::string>& tokens, bool& shouldExit) {
    // 每次执行前默认不退出，只有 exit 分支会显式改为 true。
    shouldExit = false;

    if (tokens.empty()) {
        return true;
    }

    const std::string& command = tokens[0];

    if (command == "help") {
        // 打印当前支持的内建命令列表。
        std::cout << "Supported commands:\n";
        std::cout << "help\n";
        std::cout << "pwd\n";
        std::cout << "ls\n";
        std::cout << "cd <path>\n";
        std::cout << "echo <text>\n";
        std::cout << "clear\n";
        std::cout << "exit\n";
        return true;
    }

    if (command == "pwd") {
        // 输出当前工作目录。
        std::cout << std::filesystem::current_path().string() << '\n';
        return true;
    }

    if (command == "ls") {
        // 列出当前目录下的直接子项名称。
        try {
            for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::current_path())) {
                std::cout << entry.path().filename().string() << '\n';
            }
        } catch (const std::filesystem::filesystem_error&) {
            // 保持失败提示一致，避免异常导致 Shell 崩溃。
            std::cout << "Failed to list directory\n";
        }
        return true;
    }

    if (command == "cd") {
        // 无参数时输出用法提示。
        if (tokens.size() < 2) {
            std::cout << "Usage: cd <path>\n";
            return true;
        }

        // 仅当目标路径存在且为目录时切换，否则提示目录不存在。
        try {
            const std::filesystem::path target(tokens[1]);
            if (std::filesystem::exists(target) && std::filesystem::is_directory(target)) {
                std::filesystem::current_path(target);
            } else {
                std::cout << "Directory not found\n";
            }
        } catch (const std::filesystem::filesystem_error&) {
            std::cout << "Directory not found\n";
        }
        return true;
    }

    if (command == "echo") {
        // echo 输出命令后的所有 token，并用空格拼接。
        for (std::size_t i = 1; i < tokens.size(); ++i) {
            if (i > 1) {
                std::cout << ' ';
            }
            std::cout << tokens[i];
        }
        std::cout << '\n';
        return true;
    }

    if (command == "clear") {
        // 按平台执行清屏命令，保持跨平台兼容。
#ifdef _WIN32
        std::system("cls");
#else
        std::system("clear");
#endif
        return true;
    }

    if (command == "exit") {
        // 通知 Shell 主循环退出。
        shouldExit = true;
        return true;
    }

    // 非内建命令交给调用方处理（下一轮可接外部执行）。
    return false;
}
