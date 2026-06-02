// exec_fd_test_elf_source.c：最小用户态 exec + fd 保留测试程序

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_FORK 5
#define SYS_WAITPID 6
#define SYS_EXEC_ARGS 11
#define SYS_READ 22
#define SYS_CLOSE 23
#define SYS_PIPE 40
#define SYS_DUP2 41

#define PROGRAM_EXEC_FD_WRITER 28

// 向当前 stdout 输出一段文本。
static void user_write(const char* text) {
    __asm__ __volatile__(
        "int $0x80"
        :
        : "a"(SYS_WRITE), "b"(text)
        : "memory");
}

// 结束当前用户态程序。
static void user_exit(int status) {
    __asm__ __volatile__(
        "int $0x80"
        :
        : "a"(SYS_EXIT), "b"(status)
        : "memory");

    for (;;) {
    }
}

// fork 当前进程。
static int user_fork(void) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_FORK)
        : "memory");
    return result;
}

// 等待指定子进程退出。
static int user_waitpid(int pid) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_WAITPID), "b"(pid)
        : "memory");
    return result;
}

// 以当前教学版带参数接口执行最小 exec。
static int user_exec_args(int program_id, int argc, const char* const* argv) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_EXEC_ARGS), "b"(program_id), "c"(argc), "d"(argv)
        : "memory");
    return result;
}

// 从指定 fd 读取一段文本。
static int user_read(int fd, char* buf, int size) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_READ), "b"(fd), "c"(buf), "d"(size)
        : "memory");
    return result;
}

// 关闭一个教学版 fd。
static int user_close(int fd) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_CLOSE), "b"(fd)
        : "memory");
    return result;
}

// 创建一对教学版 pipe fd。
static int user_pipe(int* fds) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_PIPE), "b"(fds)
        : "memory");
    return result;
}

// 执行教学版 dup2。
static int user_dup2(int oldfd, int newfd) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_DUP2), "b"(oldfd), "c"(newfd)
        : "memory");
    return result;
}

// 打印十进制整数，便于输出错误码。
static void user_write_int(int value) {
    char buf[16];
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
        buf[index++] = (char)('0' + (number % 10));
        number /= 10;
    }

    while (index > 0) {
        char ch[2];
        index--;
        ch[0] = buf[index];
        ch[1] = '\0';
        user_write(ch);
    }
}

// 输出统一错误前缀，便于快速定位失败步骤。
static void exec_fd_test_write_error(const char* message, int code) {
    user_write("exec_fd_test: ");
    user_write(message);
    user_write(" (");
    user_write_int(code);
    user_write(")\n");
}

// 比较两段固定长度文本；完全一致返回 0。
static int exec_fd_test_compare_text(const char* expected, const char* actual, int size) {
    int i;

    for (i = 0; i < size; i++) {
        if (expected[i] != actual[i]) {
            return -1;
        }
    }

    return 0;
}

// 主流程：验证子进程在 dup2(pipe_write_fd, 1) 之后 exec 到 writer，stdout 仍然保持为 pipe 写端。
void _start(void) {
    int fds[2];
    char buffer[64];
    const char* message = "message from exec writer\n";
    int create_result;
    int fork_result;
    int wait_result;
    int dup_result;
    int read_result;

    user_write("exec_fd_test: start\n");

    create_result = user_pipe(fds);
    if (create_result != 0) {
        exec_fd_test_write_error("pipe failed", create_result);
        user_exit(1);
    }

    fork_result = user_fork();
    if (fork_result < 0) {
        exec_fd_test_write_error("fork failed", fork_result);
        user_exit(1);
    }

    if (fork_result == 0) {
        dup_result = user_dup2(fds[1], 1);
        if (dup_result != 1) {
            user_exit(2);
        }

        user_close(fds[0]);
        user_close(fds[1]);
        if (user_exec_args(PROGRAM_EXEC_FD_WRITER, 0, (const char* const*)0) != 0) {
            user_exit(3);
        }

        user_exit(4);
    }

    wait_result = user_waitpid(fork_result);
    if (wait_result != fork_result) {
        exec_fd_test_write_error("waitpid failed", wait_result);
        user_exit(1);
    }

    read_result = user_read(fds[0], buffer, 63);
    if (read_result != 25) {
        exec_fd_test_write_error("read failed", read_result);
        user_exit(1);
    }
    buffer[read_result] = '\0';

    if (exec_fd_test_compare_text(message, buffer, 25) != 0) {
        exec_fd_test_write_error("data mismatch", 0);
        user_exit(1);
    }

    user_write("exec_fd_test: data=");
    user_write(buffer);
    user_close(fds[0]);
    user_close(fds[1]);
    user_write("exec_fd_test: stdout preserve ok\n");
    user_write("exec_fd_test: ok\n");
    user_exit(0);
}
