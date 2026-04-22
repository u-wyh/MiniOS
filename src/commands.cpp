#include "commands.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>

bool executeBuiltinCommand(const std::vector<std::string>& tokens, bool& shouldExit) {
    // 每次执行前默认不退出，只有 exit 分支会显式改为 true。
    shouldExit = false;

    if (tokens.empty()) {
        return true;
    }

    const std::string& command = tokens[0];

    if (command == "help") {
        // help 同时说明内建命令和可直接执行的系统命令。
        std::cout << "Built-in commands:\n";
        std::cout << "help pwd echo clear exit cd\n";
        std::cout << "Other system commands can be executed directly, e.g.:\n";
        std::cout << "ls\n";
        std::cout << "cat readme.md\n";
        std::cout << "uname -a\n";
        std::cout << "python3 --version\n";
        return true;
    }

    if (command == "pwd") {
        // 输出当前工作目录。
        std::cout << std::filesystem::current_path().string() << '\n';
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

    // 非内建命令交给外部执行流程处理。
    return false;
}

void executeExternalCommand(const std::vector<std::string>& tokens) {
    // 无 token 时无需执行，直接返回。
    if (tokens.empty()) {
        return;
    }

    // 组织 execvp 参数数组，末尾补 nullptr 以满足系统调用约定。
    std::vector<char*> argv;
    argv.reserve(tokens.size() + 1);
    for (const auto& token : tokens) {
        argv.push_back(const_cast<char*>(token.c_str()));
    }
    argv.push_back(nullptr);

    // fork 子进程执行外部命令，父进程等待完成后回到 Shell 提示符。
    pid_t pid = fork();
    if (pid < 0) {
        std::cout << "Failed to execute command\n";
        return;
    }

    if (pid == 0) {
        // 子进程调用 execvp，返回则代表命令不存在或执行失败。
        execvp(argv[0], argv.data());
        std::cerr << "Command not found\n";
        _exit(1);
    }

    // 父进程等待子进程，保证命令执行行为稳定且不崩溃。
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        std::cout << "Failed to execute command\n";
    }
}
