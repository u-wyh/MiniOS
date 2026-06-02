// pipe_test_elf_source.c：最小用户态 pipe 测试程序，验证 pipe() syscall 能返回一对可读写 pipe fd

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_READ 22
#define SYS_CLOSE 23
#define SYS_FD_WRITE 31
#define SYS_PIPE 40

// 最小输出 syscall 包装：把以 '\0' 结尾的字符串输出到控制台或重定向目标。
static void user_write(const char* text) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_WRITE), "b"(text) : "memory");
}

// 最小退出 syscall 包装：结束当前用户态 pipe_test 程序。
static void user_exit(int status) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_EXIT), "b"(status) : "memory");
    for (;;) {
    }
}

// 从指定 fd 读取一段文本；pipe_test 用它验证 pipe read fd 的读取与 EOF 语义。
static int user_read(int fd, char* buffer, int size) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_READ), "b"(fd), "c"(buffer), "d"(size)
                         : "memory");
    return result;
}

// 向指定 fd 写入一段文本；pipe_test 用它验证 pipe write fd 的写入路径。
static int user_fd_write(int fd, const char* buffer, int size) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_FD_WRITE), "b"(fd), "c"(buffer), "d"(size)
                         : "memory");
    return result;
}

// 关闭一个教学版 fd；这里用来清理 pipe 两端。
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

// 输出一个十进制整数，便于观察 fd 编号和 syscall 返回值。
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

// 输出统一错误提示，保持当前教学版程序风格简单稳定。
static void pipe_test_write_error(const char* message, int code) {
    user_write("pipe_test: ");
    user_write(message);
    if (code != 0) {
        user_write(" (");
        user_write_int(code);
        user_write(")");
    }
    user_write("\n");
}

// pipe_test 主流程：创建一对 pipe fd，写入一段文本，再读回并验证 EOF。
void _start(void) {
    int fds[2];
    char buffer[64];
    const char* message = "hello pipe\n";
    int create_result;
    int write_result;
    int read_result;
    int eof_result;
    int index;

    create_result = user_pipe(fds);
    if (create_result != 0) {
        pipe_test_write_error("pipe failed", create_result);
        user_exit(1);
    }

    user_write("pipe ok: read=");
    user_write_int(fds[0]);
    user_write(" write=");
    user_write_int(fds[1]);
    user_write("\n");

    write_result = user_fd_write(fds[1], message, 11);
    if (write_result < 0) {
        pipe_test_write_error("write failed", write_result);
        user_close(fds[0]);
        user_close(fds[1]);
        user_exit(1);
    }

    read_result = user_read(fds[0], buffer, 63);
    if (read_result < 0) {
        pipe_test_write_error("read failed", read_result);
        user_close(fds[0]);
        user_close(fds[1]);
        user_exit(1);
    }

    for (index = 0; index < read_result && index < 63; index++) {
        buffer[index] = buffer[index];
    }
    buffer[read_result] = '\0';

    user_write("pipe data: ");
    user_write(buffer);

    eof_result = user_read(fds[0], buffer, 63);
    user_write("pipe eof: ");
    user_write_int(eof_result);
    user_write("\n");

    user_close(fds[0]);
    user_close(fds[1]);
    user_exit(0);
}
