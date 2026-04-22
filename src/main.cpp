#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    std::string input;

    // 主循环：持续读取用户输入，后续可在这里扩展命令表与调度逻辑。
    while (true) {
        std::cout << "MiniOS> ";
        std::getline(std::cin, input);

        // 输入流结束时直接退出，避免死循环。
        if (!std::cin) {
            break;
        }

        // 解析命令词与参数，命令词放在第一个 token。
        std::istringstream iss(input);
        std::string command;
        iss >> command;

        // 空输入直接进入下一轮，保持交互体验。
        if (command.empty()) {
            continue;
        }

        if (command == "help") {
            // 打印内置命令列表，便于用户快速查看功能。
            std::cout << "Supported commands:\n";
            std::cout << "help\n";
            std::cout << "pwd\n";
            std::cout << "ls\n";
            std::cout << "cd <path>\n";
            std::cout << "echo <text>\n";
            std::cout << "clear\n";
            std::cout << "exit\n";
        } else if (command == "pwd") {
            // 输出当前工作目录，作为后续文件系统模块的基础能力。
            std::cout << std::filesystem::current_path().string() << '\n';
        } else if (command == "ls") {
            // 列出当前目录下的直接子项名称（文件或目录）。
            try {
                for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::current_path())) {
                    std::cout << entry.path().filename().string() << '\n';
                }
            } catch (const std::filesystem::filesystem_error&) {
                // 目录读取失败时给出清晰提示，避免程序崩溃。
                std::cout << "Failed to list directory\n";
            }
        } else if (command == "cd") {
            // 读取目标路径参数；无参数时按规范输出用法提示。
            std::string path;
            iss >> path;
            if (path.empty()) {
                std::cout << "Usage: cd <path>\n";
                continue;
            }

            // 仅在目标存在且为目录时切换，错误输入不影响 Shell 主循环。
            try {
                const std::filesystem::path target(path);
                if (std::filesystem::exists(target) && std::filesystem::is_directory(target)) {
                    std::filesystem::current_path(target);
                } else {
                    std::cout << "Directory not found\n";
                }
            } catch (const std::filesystem::filesystem_error&) {
                std::cout << "Directory not found\n";
            }
        } else if (command == "echo") {
            // 保留命令后的原始参数内容（去掉一个前导空格后输出）。
            std::string args;
            std::getline(iss, args);
            if (!args.empty() && args[0] == ' ') {
                args.erase(0, 1);
            }
            std::cout << args << '\n';
        } else if (command == "clear") {
            // 按平台执行清屏命令，保证 Windows/Linux 兼容。
#ifdef _WIN32
            std::system("cls");
#else
            std::system("clear");
#endif
        } else if (command == "exit") {
            // 退出 Shell 循环，结束程序。
            break;
        } else {
            // 未知命令统一提示，便于后续扩展命令注册机制。
            std::cout << "Unknown command\n";
        }
    }

    return 0;
}
