#include "exec.h"
#include "process.h"
#include "vga.h"

// 最小待执行请求缓冲区：保存 shell 提交的 run <file> 名称
static char exec_pending_name[32];
static int exec_pending = 0;

// 复制程序名到挂起缓冲区，避免直接依赖键盘输入缓冲
static void exec_copy_name(char* dst, const char* src, int max_len) {
    int i = 0;

    while (i + 1 < max_len && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

// 按文件名执行用户程序：创建进程并切换到该进程运行
void exec(const char* name) {
    struct process* proc;

    proc = process_create(name);
    if (proc == (struct process*)0) {
        print_string("exec: create process failed\n");
        return;
    }

    process_run(proc);
}

// 登记一次待执行请求，让真正的 exec 在内核主循环中发生
void exec_request(const char* name) {
    if (name == (const char*)0 || name[0] == '\0') {
        print_string("exec: empty name\n");
        return;
    }

    exec_copy_name(exec_pending_name, name, 32);
    exec_pending = 1;
}

// 查询是否存在待执行请求
int exec_has_pending_request(void) {
    return exec_pending;
}

// 取出并执行当前待处理请求
void exec_run_pending(void) {
    if (exec_pending == 0) {
        return;
    }

    exec_pending = 0;
    exec(exec_pending_name);
}
