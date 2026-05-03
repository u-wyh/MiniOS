// shell.h：声明内核命令行初始化与命令执行入口
#ifndef SHELL_H
#define SHELL_H

// 初始化最小 Shell，并打印首个命令提示符
void shell_init(void);
// 执行一行命令并在结束后重新打印提示符
void shell_execute(const char* line);

#endif
