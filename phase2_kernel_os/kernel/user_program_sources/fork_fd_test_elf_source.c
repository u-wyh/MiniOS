// fork_fd_test_elf_source.c：最小用户态 fork + pipe 继承测试程序，验证子进程能继承父进程 pipe fd

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_FORK 5
#define SYS_WAITPID 6
#define SYS_READ 22
#define SYS_CLOSE 23
#define SYS_FD_WRITE 31
#define SYS_PIPE 40

// 最小输出 syscall 包装：把字符串输出到控制台。
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

// 从指定 fd 读取一段文本。
static int user_read(int fd, char* buffer, int size) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_READ), "b"(fd), "c"(buffer), "d"(size)
                         : "memory");
    return result;
}

// 向指定 fd 写入一段文本。
static int user_fd_write(int fd, const char* buffer, int size) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_FD_WRITE), "b"(fd), "c"(buffer), "d"(size)
                         : "memory");
    return result;
}

// 关闭一个教学版 fd。
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

// 输出十进制整数，便于观察 pid 与 syscall 返回值。
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

// 输出统一错误提示，保持教学版测试程序风格一致。
static void fork_fd_test_write_error(const char* message, int code) {
    user_write("fork_fd_test: ");
    user_write(message);
    if (code != 0) {
        user_write(" (");
        user_write_int(code);
        user_write(")");
    }
    user_write("\n");
}

// 比较固定长度文本，完全一致返回 0。
static int fork_fd_test_compare_text(const char* expected, const char* actual, int size) {
    int index = 0;

    while (index < size) {
        if (expected[index] != actual[index]) {
            return -1;
        }
        index++;
    }

    return 0;
}

// 主流程：父进程创建 pipe 后 fork，子进程用继承下来的写端写入，父进程 wait 后读取。
void _start(void) {
    int fds[2];
    char buffer[64];
    const char* message = "child says hello\n";
    int create_result;
    int fork_result;
    int wait_result;
    int read_result;

    user_write("fork_fd_test: start\n");

    create_result = user_pipe(fds);
    if (create_result != 0) {
        fork_fd_test_write_error("pipe failed", create_result);
        user_exit(1);
    }

    fork_result = user_fork();
    if (fork_result < 0) {
        fork_fd_test_write_error("fork failed", fork_result);
        user_exit(1);
    }

    if (fork_result == 0) {
        if (user_fd_write(fds[1], message, 17) != 17) {
            user_exit(2);
        }
        user_close(fds[0]);
        user_close(fds[1]);
        user_exit(0);
    }

    wait_result = user_waitpid(fork_result);
    if (wait_result != fork_result) {
        fork_fd_test_write_error("waitpid failed", wait_result);
        user_exit(1);
    }

    read_result = user_read(fds[0], buffer, 63);
    if (read_result != 17) {
        fork_fd_test_write_error("read failed", read_result);
        user_exit(1);
    }
    buffer[read_result] = '\0';
    if (fork_fd_test_compare_text(message, buffer, 17) != 0) {
        fork_fd_test_write_error("data mismatch", 0);
        user_exit(1);
    }

    user_write("fork_fd_test: data=");
    user_write(buffer);
    user_close(fds[0]);
    user_close(fds[1]);
    user_write("fork_fd_test: ok\n");
    user_exit(0);
}
