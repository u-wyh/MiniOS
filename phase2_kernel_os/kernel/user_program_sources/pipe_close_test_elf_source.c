// pipe_close_test_elf_source.c：最小用户态 pipe close 语义测试程序

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_READ 22
#define SYS_CLOSE 23
#define SYS_PIPE 40
#define SYS_FD_WRITE 31

// 最小输出 syscall 包装：把字符串写到当前 stdout。
static void user_write(const char* text) {
    __asm__ __volatile__(
        "int $0x80"
        :
        : "a"(SYS_WRITE), "b"(text)
        : "memory");
}

// 最小退出 syscall 包装：结束当前用户态测试程序。
static void user_exit(int status) {
    __asm__ __volatile__(
        "int $0x80"
        :
        : "a"(SYS_EXIT), "b"(status)
        : "memory");

    for (;;) {
    }
}

// 从指定 fd 读取一段文本；返回读取字节数或负值错误码。
static int user_read(int fd, char* buf, int size) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_READ), "b"(fd), "c"(buf), "d"(size)
        : "memory");
    return result;
}

// 向指定 fd 写入一段文本；返回写入字节数或负值错误码。
static int user_fd_write(int fd, const char* buf, int size) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_FD_WRITE), "b"(fd), "c"(buf), "d"(size)
        : "memory");
    return result;
}

// 关闭一个教学版 fd；返回 0 或负值错误码。
static int user_close(int fd) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_CLOSE), "b"(fd)
        : "memory");
    return result;
}

// 最小 pipe syscall 包装：成功返回 0，并把读端/写端写入 fds[0]/fds[1]。
static int user_pipe(int* fds) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_PIPE), "b"(fds)
        : "memory");
    return result;
}

// 打印一个十进制整数，便于把错误码写到屏幕。
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

// 写出统一错误前缀，便于在屏幕上快速定位是哪一项失败。
static void pipe_close_test_write_error(const char* message, int code) {
    user_write("pipe_close_test: ");
    user_write(message);
    user_write(" (");
    user_write_int(code);
    user_write(")\n");
}

// 比较两段固定长度文本；完全一致返回 0。
static int pipe_close_test_compare_text(const char* expected, const char* actual, int size) {
    int i;

    for (i = 0; i < size; i++) {
        if (expected[i] != actual[i]) {
            return -1;
        }
    }

    return 0;
}

// 用例一：关闭写端后，读端应先读出已有数据，再读到 EOF。
static int pipe_close_test_case_write_close_eof(void) {
    int fds[2];
    char buffer[8];
    const char* text = "abc\n";
    int result;

    result = user_pipe(fds);
    if (result != 0) {
        return -1;
    }

    result = user_fd_write(fds[1], text, 4);
    if (result != 4) {
        return -2;
    }

    result = user_close(fds[1]);
    if (result != 0) {
        return -3;
    }

    result = user_read(fds[0], buffer, 7);
    if (result != 4) {
        return -4;
    }

    if (pipe_close_test_compare_text(text, buffer, 4) != 0) {
        return -5;
    }

    result = user_read(fds[0], buffer, 7);
    if (result != 0) {
        return -6;
    }

    user_close(fds[0]);
    return 0;
}

// 用例二：关闭读端后继续 write，不应 panic；当前教学版允许返回错误或 0。
static int pipe_close_test_case_read_close_write_handled(void) {
    int fds[2];
    int result;

    result = user_pipe(fds);
    if (result != 0) {
        return -1;
    }

    result = user_close(fds[0]);
    if (result != 0) {
        return -2;
    }

    result = user_fd_write(fds[1], "x", 1);
    if (result > 0) {
        return -3;
    }

    user_close(fds[1]);
    return 0;
}

// 用例三：重复 close 同一个 fd，不应 panic；当前教学版第二次 close 返回负值即可。
static int pipe_close_test_case_double_close(void) {
    int fds[2];
    int result;

    result = user_pipe(fds);
    if (result != 0) {
        return -1;
    }

    result = user_close(fds[0]);
    if (result != 0) {
        return -2;
    }

    result = user_close(fds[0]);
    if (result >= 0) {
        return -3;
    }

    user_close(fds[1]);
    return 0;
}

// 主流程：顺序验证教学版 pipe close 的三类最小语义。
void _start(void) {
    int result;

    user_write("pipe_close_test: start\n");

    result = pipe_close_test_case_write_close_eof();
    if (result != 0) {
        pipe_close_test_write_error("write close eof failed", result);
        user_exit(1);
    }
    user_write("pipe_close_test: write close eof ok\n");

    result = pipe_close_test_case_read_close_write_handled();
    if (result != 0) {
        pipe_close_test_write_error("read close write failed", result);
        user_exit(1);
    }
    user_write("pipe_close_test: read close write handled\n");

    result = pipe_close_test_case_double_close();
    if (result != 0) {
        pipe_close_test_write_error("double close failed", result);
        user_exit(1);
    }
    user_write("pipe_close_test: double close handled\n");

    user_write("pipe_close_test: ok\n");
    user_exit(0);
}
