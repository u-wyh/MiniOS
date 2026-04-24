#include "commands.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <algorithm>
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

bool executePipeCommand(const std::vector<std::string>& tokens) {
    // 仅当命令中包含管道符时才处理；否则交回普通流程。
    const int pipe_count = static_cast<int>(std::count(tokens.begin(), tokens.end(), "|"));
    if (pipe_count == 0) {
        return false;
    }

    // 本轮只支持一个管道符，多于一个直接提示非法输入。
    if (pipe_count > 1) {
        std::cout << "Invalid pipe command\n";
        return true;
    }

    // 定位管道符并拆分左右命令参数。
    const auto pipe_it = std::find(tokens.begin(), tokens.end(), "|");
    const std::size_t pipe_pos = static_cast<std::size_t>(pipe_it - tokens.begin());

    if (pipe_pos == 0 || pipe_pos == tokens.size() - 1) {
        std::cout << "Invalid pipe command\n";
        return true;
    }

    const std::vector<std::string> left(tokens.begin(), tokens.begin() + static_cast<long>(pipe_pos));
    const std::vector<std::string> right(tokens.begin() + static_cast<long>(pipe_pos) + 1, tokens.end());

    if (left.empty() || right.empty()) {
        std::cout << "Invalid pipe command\n";
        return true;
    }

    // 创建管道，连接 left 的 stdout 到 right 的 stdin。
    int pipe_fd[2];
    if (pipe(pipe_fd) < 0) {
        std::cout << "Failed to execute command\n";
        return true;
    }

    pid_t left_pid = fork();
    if (left_pid < 0) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        std::cout << "Failed to execute command\n";
        return true;
    }

    if (left_pid == 0) {
        // 左子进程：把标准输出重定向到管道写端后执行 left 命令。
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[0]);
        close(pipe_fd[1]);

        std::vector<char*> left_argv;
        left_argv.reserve(left.size() + 1);
        for (const auto& token : left) {
            left_argv.push_back(const_cast<char*>(token.c_str()));
        }
        left_argv.push_back(nullptr);

        execvp(left_argv[0], left_argv.data());
        std::cerr << "Command not found\n";
        _exit(1);
    }

    pid_t right_pid = fork();
    if (right_pid < 0) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        waitpid(left_pid, nullptr, 0);
        std::cout << "Failed to execute command\n";
        return true;
    }

    if (right_pid == 0) {
        // 右子进程：把标准输入重定向到管道读端后执行 right 命令。
        dup2(pipe_fd[0], STDIN_FILENO);
        close(pipe_fd[0]);
        close(pipe_fd[1]);

        std::vector<char*> right_argv;
        right_argv.reserve(right.size() + 1);
        for (const auto& token : right) {
            right_argv.push_back(const_cast<char*>(token.c_str()));
        }
        right_argv.push_back(nullptr);

        execvp(right_argv[0], right_argv.data());
        std::cerr << "Command not found\n";
        _exit(1);
    }

    // 父进程关闭管道端并等待两个子进程，保证 Shell 稳定返回提示符。
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    waitpid(left_pid, nullptr, 0);
    waitpid(right_pid, nullptr, 0);
    return true;
}
