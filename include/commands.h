#ifndef MINIOS_COMMANDS_H
#define MINIOS_COMMANDS_H

#include <string>
#include <vector>

// 统一处理内建命令：返回 true 表示已处理，false 表示不是内建命令。
bool executeBuiltinCommand(const std::vector<std::string>& tokens, bool& shouldExit);

// 执行外部命令（非内建命令），失败时输出清晰错误提示。
void executeExternalCommand(const std::vector<std::string>& tokens);

// 执行单管道命令：返回 true 表示本次输入属于管道场景并已处理。
bool executePipeCommand(const std::vector<std::string>& tokens);

// 执行输出重定向命令：返回 true 表示本次输入属于重定向场景并已处理。
bool executeRedirectCommand(const std::vector<std::string>& tokens);

#endif
