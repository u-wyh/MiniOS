#ifndef MINIOS_COMMANDS_H
#define MINIOS_COMMANDS_H

#include <string>
#include <vector>

// 统一处理内建命令：返回 true 表示已处理，false 表示不是内建命令。
bool executeBuiltinCommand(const std::vector<std::string>& tokens, bool& shouldExit);

#endif
