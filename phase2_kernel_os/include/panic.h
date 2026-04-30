#ifndef PANIC_H
#define PANIC_H

// 输出 panic 信息并关闭中断停机，用于最小内核致命错误处理
void kernel_panic(const char* message);

#endif
