// pipe_multi_test_elf_source.c：最小用户态多 pipe object 隔离测试程序

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_READ 22
#define SYS_CLOSE 23
#define SYS_FD_WRITE 31
#define SYS_PIPE 40

// 最小输出 syscall 包装：把字符串写到当前 stdout。
static void user_write(const char* text) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_WRITE), "b"(text) : "memory");
}

// 最小退出 syscall 包装：结束当前测试程序。
static void user_exit(int status) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_EXIT), "b"(status) : "memory");
    for (;;) {
    }
}

// 从指定 fd 读取一段文本；返回读取字节数或负值错误码。
static int user_read(int fd, char* buffer, int size) {
    int result;

    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_READ), "b"(fd), "c"(buffer), "d"(size)
                         : "memory");
    return result;
}

// 向指定 fd 写入一段文本；返回写入字节数或负值错误码。
static int user_fd_write(int fd, const char* buffer, int size) {
    int result;

    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_FD_WRITE), "b"(fd), "c"(buffer), "d"(size)
                         : "memory");
    return result;
}

// 关闭一个教学版 fd；返回 0 或负值错误码。
static int user_close(int fd) {
    int result;

    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_CLOSE), "b"(fd)
                         : "memory");
    return result;
}

// 最小 pipe syscall 包装：成功时返回 0，并把读端/写端写回 fds[0]/fds[1]。
static int user_pipe(int* fds) {
    int result;

    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_PIPE), "b"(fds)
                         : "memory");
    return result;
}

// 输出十进制整数，便于把错误码写出来。
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

// 输出统一错误提示，方便快速定位是哪一步失败。
static void pipe_multi_test_write_error(const char* message, int code) {
    user_write("pipe_multi_test: ");
    user_write(message);
    if (code != 0) {
        user_write(" (");
        user_write_int(code);
        user_write(")");
    }
    user_write("\n");
}

// 比较两段固定长度文本；完全一致返回 0。
static int pipe_multi_test_compare_text(const char* expected, const char* actual, int size) {
    int i;

    for (i = 0; i < size; i++) {
        if (expected[i] != actual[i]) {
            return -1;
        }
    }

    return 0;
}

// 读取指定 pipe 的固定长度文本并校验内容，用来确认不同 pipe object 不会串数据。
static int pipe_multi_test_expect_read(int fd, const char* expected, int expected_len) {
    char buffer[32];
    int read_result;

    read_result = user_read(fd, buffer, 31);
    if (read_result != expected_len) {
        return -1;
    }

    if (pipe_multi_test_compare_text(expected, buffer, expected_len) != 0) {
        return -2;
    }

    return 0;
}

// 主流程：同时创建两组 pipe，写入不同文本，再确认读回数据互不污染，最后验证 close 后对象可复用。
void _start(void) {
    int pipe_a[2];
    int pipe_b[2];
    int pipe_c[2];
    const char* text_a = "AAA\n";
    const char* text_b = "BBB\n";
    const char* text_c = "CCC\n";
    int result;

    user_write("pipe_multi_test: start\n");

    result = user_pipe(pipe_a);
    if (result != 0) {
        pipe_multi_test_write_error("create pipe A failed", result);
        user_exit(1);
    }

    result = user_pipe(pipe_b);
    if (result != 0) {
        pipe_multi_test_write_error("create pipe B failed", result);
        user_exit(1);
    }

    result = user_fd_write(pipe_a[1], text_a, 4);
    if (result != 4) {
        pipe_multi_test_write_error("write pipe A failed", result);
        user_exit(1);
    }

    result = user_fd_write(pipe_b[1], text_b, 4);
    if (result != 4) {
        pipe_multi_test_write_error("write pipe B failed", result);
        user_exit(1);
    }

    result = pipe_multi_test_expect_read(pipe_a[0], text_a, 4);
    if (result != 0) {
        pipe_multi_test_write_error("read pipe A mismatch", result);
        user_exit(1);
    }

    result = pipe_multi_test_expect_read(pipe_b[0], text_b, 4);
    if (result != 0) {
        pipe_multi_test_write_error("read pipe B mismatch", result);
        user_exit(1);
    }

    user_write("pipe_multi_test: multi-pipe isolation ok\n");

    user_close(pipe_a[0]);
    user_close(pipe_a[1]);
    user_close(pipe_b[0]);
    user_close(pipe_b[1]);

    result = user_pipe(pipe_c);
    if (result != 0) {
        pipe_multi_test_write_error("recreate pipe C failed", result);
        user_exit(1);
    }

    result = user_fd_write(pipe_c[1], text_c, 4);
    if (result != 4) {
        pipe_multi_test_write_error("write pipe C failed", result);
        user_exit(1);
    }

    result = pipe_multi_test_expect_read(pipe_c[0], text_c, 4);
    if (result != 0) {
        pipe_multi_test_write_error("read pipe C mismatch", result);
        user_exit(1);
    }

    user_close(pipe_c[0]);
    user_close(pipe_c[1]);
    user_write("pipe_multi_test: pipe reuse ok\n");
    user_write("pipe_multi_test: ok\n");
    user_exit(0);
}
