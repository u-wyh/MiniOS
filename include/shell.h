#ifndef MINIOS_SHELL_H
#define MINIOS_SHELL_H

#include <string>
#include <vector>

// Shell 类负责主循环与输入解析，不直接承载具体命令实现。
class Shell {
public:
    // 启动 Shell 主循环。
    void run();

private:
    // 将用户输入按空白符切分为 token，供命令分发使用。
    std::vector<std::string> parseInput(const std::string& input) const;
};

#endif
