/* ===== 最小 syscall 包装层 ===== */

// 最小 int 0x80 包装：保持和当前教学版用户程序 ABI 一致。
static int user_syscall0(int number) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(number) : "memory");
    return result;
}

// 一个参数的系统调用封装。
static int user_syscall1(int number, int arg1) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(number), "b"(arg1) : "memory");
    return result;
}

// 两个参数的系统调用封装。
static int user_syscall2(int number, int arg1, int arg2) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2) : "memory");
    return result;
}

// 三个参数的系统调用封装。
static int user_syscall3(int number, int arg1, int arg2, int arg3) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3) : "memory");
    return result;
}

// 向控制台输出字符串。
static void user_write(const char* text) {
    user_syscall1(SYS_WRITE, (int)text);
}

// 退出当前用户进程。
static void user_exit(int status) {
    user_syscall1(SYS_EXIT, status);
    for (;;) {
    }
}

// 读取一个字符；无输入时内核会阻塞当前 shell。
static int user_read_char(void) {
    return user_syscall0(SYS_READ_CHAR);
}

// 让当前进程睡眠若干 tick。
static int user_sleep_ticks(unsigned int ticks) {
    return user_syscall1(SYS_SLEEP, (int)ticks);
}

// 教学调试接口：按 pid 让目标进程睡眠。
static int user_sleep_pid(int pid, unsigned int ticks) {
    return user_syscall2(SYS_SLEEP_PID, pid, (int)ticks);
}

// 读取系统启动以来的 tick 数。
static unsigned int user_get_ticks(void) {
    return (unsigned int)user_syscall0(SYS_GET_TICKS);
}

// 创建子进程。
static int user_fork(void) {
    return user_syscall0(SYS_FORK);
}

// 等待指定子进程退出并回收。
static int user_waitpid(int pid) {
    return user_syscall1(SYS_WAITPID, pid);
}

// 非阻塞回收任意一个已经退出的子进程。
static int user_wait_any(void) {
    return user_syscall0(SYS_WAIT_ANY);
}

// 以带参数方式执行教学版最小 exec。
static int user_exec_args(int program_id, int argc, const char* const* argv) {
    return user_syscall3(SYS_EXEC_ARGS, program_id, argc, (int)argv);
}

// 逐条读取 ps 摘要。
static int user_ps_get(int index, struct process_info* info) {
    return user_syscall2(SYS_PS, index, (int)info);
}

// 终止指定 pid。
static int user_kill(int pid) {
    return user_syscall1(SYS_KILL, pid);
}

// 设置后台标记，供 start 命令使用。
static int user_set_background(int pid, int is_background) {
    return user_syscall2(SYS_SET_BACKGROUND, pid, is_background);
}

// 请求内核清空当前 VGA 文本屏幕，供用户态 clear 命令复用。
static void user_clear_screen(void) {
    user_syscall0(SYS_CLEAR_SCREEN);
}

// 打开一个教学版只读文件，成功返回 fd。
static int user_open(const char* path) {
    return user_syscall1(SYS_OPEN, (int)path);
}

// 从 fd 读取最多 size 字节到用户缓冲区，成功返回字节数，EOF 返回 0。
static int user_read(int fd, char* buffer, int size) {
    return user_syscall3(SYS_READ, fd, (int)buffer, size);
}

// 关闭一个已打开 fd。
static int user_close(int fd) {
    return user_syscall1(SYS_CLOSE, fd);
}

// 查询当前内置只读文件数量；供用户态 ls 程序与 shell 共享同一套教学版文件列表语义。
static int user_file_count(void) {
    return user_syscall0(SYS_FILE_COUNT);
}

// 按索引读取内置只读文件路径，并返回该文件大小。
static int user_file_info(int index, char* buffer, int max_len) {
    return user_syscall3(SYS_FILE_INFO, index, (int)buffer, max_len);
}

// 创建一个空 RAMFS 文件；供 touch 命令复用。
static int user_touch(const char* path) {
    return user_syscall1(SYS_TOUCH, (int)path);
}

// 覆盖写入一个 RAMFS 文本文件；供 writefile 命令复用。
static int user_writefile(const char* path, const char* text) {
    return user_syscall2(SYS_WRITEFILE, (int)path, (int)text);
}

// 删除一个 RAMFS 文件；供 rm 命令复用。
static int user_rm(const char* path) {
    return user_syscall1(SYS_RM, (int)path);
}

// 追加写入一个 RAMFS 文本文件；供 append 命令复用。
static int user_appendfile(const char* path, const char* text) {
    return user_syscall2(SYS_APPEND_FILE, (int)path, (int)text);
}

// 为指定 pid 的子进程启用教学版 stdout 重定向；后续该子进程的 SYS_WRITE 会根据配置写入 RAMFS。
static int user_set_stdout_redirect(int pid, const char* path, int is_append) {
    return user_syscall3(SYS_SET_STDOUT_REDIRECT, pid, (int)path, is_append);
}

// 为指定 pid 的子进程启用教学版 stdin 重定向；后续该子进程的 SYS_READ(fd=0) 会从文件读取。
static int user_set_stdin_redirect(int pid, const char* path) {
    return user_syscall2(SYS_SET_STDIN_REDIRECT, pid, (int)path);
}

// 清空教学版单管道缓冲区，供 shell 在执行 run A | run B 前后重置状态。
static void user_pipe_reset(void) {
    user_syscall0(SYS_PIPE_RESET);
}

// 把指定 pid 的 stdout 改为写入教学版 pipe buffer。
static int user_set_stdout_pipe(int pid) {
    return user_syscall1(SYS_SET_STDOUT_PIPE, pid);
}

// 把指定 pid 的 stdin 改为从教学版 pipe buffer 读取。
static int user_set_stdin_pipe(int pid) {
    return user_syscall1(SYS_SET_STDIN_PIPE, pid);
}

// 通知内核：该 shell 子进程的重定向/pipe 配置已经完成，可以开始真正执行用户态子分支。
static int user_set_launch_ready(int pid) {
    return user_syscall1(SYS_SET_LAUNCH_READY, pid);
}

// 当前 shell 子分支在 exec 前轮询该标志，避免父进程还没挂好重定向时就提前开始读写。
static int user_get_launch_ready(void) {
    return user_syscall0(SYS_GET_LAUNCH_READY);
}

// 查询一个当前可见文件的教学版元信息；供 shell 在真正启动子进程前预检查重定向目标是否合法。
static int user_stat(const char* path, struct minios_stat* out_stat) {
    return user_syscall2(SYS_STAT, (int)path, (int)out_stat);
}

// 主动让出 CPU，供 launch_ready 等门闩场景在父进程补齐配置前短暂等待。
static int user_yield(void) {
    return user_syscall0(SYS_YIELD);
}

