// pipeline_demo_elf_source.c：最小用户态 pipeline demo，验证 pipe + fork + dup2 + exec 端到端链路

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_FORK 5
#define SYS_WAITPID 6
#define SYS_EXEC_ARGS 11
#define SYS_CLOSE 23
#define SYS_PIPE 40
#define SYS_DUP2 41

#define PROGRAM_PIPELINE_WRITER 31
#define PROGRAM_PIPELINE_READER 32

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

// 输出统一错误提示，便于快速定位哪一步失败。
static void pipeline_demo_write_error(const char* message, int code) {
    user_write("pipeline_demo: ");
    user_write(message);
    user_write(" (");
    user_write_int(code);
    user_write(")\n");
}

// 主流程：采用教学版顺序模型，先跑 writer，再跑 reader，避免依赖并发阻塞 pipe。
void _start(void) {
    int fds[2];
    int pipe_result;
    int writer_pid;
    int reader_pid;
    int wait_result;
    int dup_result;

    user_write("pipeline_demo: start\n");

    pipe_result = user_pipe(fds);
    if (pipe_result != 0) {
        pipeline_demo_write_error("pipe failed", pipe_result);
        user_exit(1);
    }

    writer_pid = user_fork();
    if (writer_pid < 0) {
        pipeline_demo_write_error("fork writer failed", writer_pid);
        user_exit(1);
    }

    if (writer_pid == 0) {
        dup_result = user_dup2(fds[1], 1);
        if (dup_result != 1) {
            user_exit(2);
        }

        user_close(fds[0]);
        user_close(fds[1]);
        if (user_exec_args(PROGRAM_PIPELINE_WRITER, 0, (const char* const*)0) != 0) {
            user_exit(3);
        }

        user_exit(4);
    }

    wait_result = user_waitpid(writer_pid);
    if (wait_result != writer_pid) {
        pipeline_demo_write_error("wait writer failed", wait_result);
        user_exit(1);
    }

    reader_pid = user_fork();
    if (reader_pid < 0) {
        pipeline_demo_write_error("fork reader failed", reader_pid);
        user_exit(1);
    }

    if (reader_pid == 0) {
        dup_result = user_dup2(fds[0], 0);
        if (dup_result != 0) {
            user_exit(5);
        }

        user_close(fds[0]);
        user_close(fds[1]);
        if (user_exec_args(PROGRAM_PIPELINE_READER, 0, (const char* const*)0) != 0) {
            user_exit(6);
        }

        user_exit(7);
    }

    wait_result = user_waitpid(reader_pid);
    if (wait_result != reader_pid) {
        pipeline_demo_write_error("wait reader failed", wait_result);
        user_exit(1);
    }

    user_close(fds[0]);
    user_close(fds[1]);
    user_write("pipeline_demo: ok\n");
    user_exit(0);
}
