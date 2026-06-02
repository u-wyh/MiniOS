// pipe_fork_dup2_test_elf_source.c：最小用户态 pipe + fork + dup2 组合测试程序

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_FORK 5
#define SYS_WAITPID 6
#define SYS_READ 22
#define SYS_CLOSE 23
#define SYS_PIPE 40
#define SYS_DUP2 41

// 最小输出 syscall 包装：把字符串输出到当前 stdout。
static void user_write(const char* text) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_WRITE), "b"(text) : "memory");
}

// 最小退出 syscall 包装：结束当前用户态程序。
static void user_exit(int status) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_EXIT), "b"(status) : "memory");
    for (;;) {
    }
}

// 最小 fork syscall 包装：父进程返回子 pid，子进程返回 0。
static int user_fork(void) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_FORK) : "memory");
    return result;
}

// 最小 waitpid syscall 包装：等待指定子进程结束。
static int user_waitpid(int pid) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_WAITPID), "b"(pid) : "memory");
    return result;
}

// 从指定 fd 读取一段文本；父进程用它从 dup2 后的 stdin 读回子进程写入的消息。
static int user_read(int fd, char* buffer, int size) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_READ), "b"(fd), "c"(buffer), "d"(size)
                         : "memory");
    return result;
}

// 关闭一个教学版 fd；用于测试末尾清理 pipe 两端。
static int user_close(int fd) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_CLOSE), "b"(fd) : "memory");
    return result;
}

// 最小 pipe syscall 包装：成功时返回 0，并把读端/写端写入 fds[0]/fds[1]。
static int user_pipe(int* fds) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_PIPE), "b"(fds) : "memory");
    return result;
}

// 最小 dup2 syscall 包装：成功时返回 newfd，失败返回 -1。
static int user_dup2(int oldfd, int newfd) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_DUP2), "b"(oldfd), "c"(newfd)
                         : "memory");
    return result;
}

// 输出十进制整数，便于输出 fork / wait / dup2 错误码。
static void user_write_int(int value) {
    char digits[16];
    int index = 0;
    unsigned int number;

    if (value == 0) {
        user_write("0");
        return;
    }

    if (value < 0) {
        user_write("-");
        number = (unsigned int)(-value);
    } else {
        number = (unsigned int)value;
    }

    while (number > 0) {
        digits[index++] = (char)('0' + (number % 10));
        number /= 10;
    }

    while (index > 0) {
        char one[2];
        index--;
        one[0] = digits[index];
        one[1] = '\0';
        user_write(one);
    }
}

// 输出统一错误提示，保持当前教学版测试程序风格。
static void pipe_fork_dup2_test_write_error(const char* message, int code) {
    user_write("pipe_fork_dup2_test: ");
    user_write(message);
    if (code != 0) {
        user_write(" (");
        user_write_int(code);
        user_write(")");
    }
    user_write("\n");
}

// 比较固定长度文本，完全一致返回 0。
static int pipe_fork_dup2_test_compare_text(const char* expected, const char* actual, int size) {
    int index = 0;

    while (index < size) {
        if (expected[index] != actual[index]) {
            return -1;
        }
        index++;
    }

    return 0;
}

// 主流程：用户态自己组合 pipe + fork + dup2，验证最小真实管道模型闭环。
void _start(void) {
    int fds[2];
    char buffer[64];
    const char* message = "message from child\n";
    int pipe_result;
    int fork_result;
    int wait_result;
    int dup_result;
    int read_result;

    user_write("pipe_fork_dup2_test: start\n");

    pipe_result = user_pipe(fds);
    if (pipe_result != 0) {
        pipe_fork_dup2_test_write_error("pipe failed", pipe_result);
        user_exit(1);
    }

    fork_result = user_fork();
    if (fork_result < 0) {
        pipe_fork_dup2_test_write_error("fork failed", fork_result);
        user_exit(1);
    }

    if (fork_result == 0) {
        dup_result = user_dup2(fds[1], 1);
        if (dup_result != 1) {
            user_exit(2);
        }

        // dup2 后，stdout 已经指向 pipe 写端；这段输出会进入 pipe，而不是直接显示到屏幕。
        user_write(message);
        user_close(fds[0]);
        user_close(fds[1]);
        user_exit(0);
    }

    wait_result = user_waitpid(fork_result);
    if (wait_result != fork_result) {
        pipe_fork_dup2_test_write_error("waitpid failed", wait_result);
        user_exit(1);
    }

    dup_result = user_dup2(fds[0], 0);
    if (dup_result != 0) {
        pipe_fork_dup2_test_write_error("dup read fd failed", dup_result);
        user_exit(1);
    }

    read_result = user_read(0, buffer, 63);
    if (read_result != 19) {
        pipe_fork_dup2_test_write_error("read failed", read_result);
        user_exit(1);
    }
    buffer[read_result] = '\0';

    if (pipe_fork_dup2_test_compare_text(message, buffer, 19) != 0) {
        pipe_fork_dup2_test_write_error("data mismatch", 0);
        user_exit(1);
    }

    user_write("pipe_fork_dup2_test: data=");
    user_write(buffer);
    user_close(fds[0]);
    user_close(fds[1]);
    user_write("pipe_fork_dup2_test: ok\n");
    user_exit(0);
}
