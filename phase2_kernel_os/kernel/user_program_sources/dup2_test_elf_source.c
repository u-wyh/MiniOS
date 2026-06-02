// dup2_test_elf_source.c：最小用户态 dup2 测试程序，验证教学版 dup2() syscall 能复制 pipe fd

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_OPEN 21
#define SYS_READ 22
#define SYS_CLOSE 23
#define SYS_FD_WRITE 31
#define SYS_PIPE 40
#define SYS_DUP2 41

// 最小输出 syscall 包装：把以 '\0' 结尾的字符串输出到当前 stdout 或控制台。
static void user_write(const char* text) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_WRITE), "b"(text) : "memory");
}

// 最小退出 syscall 包装：结束当前用户态 dup2_test 程序。
static void user_exit(int status) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_EXIT), "b"(status) : "memory");
    for (;;) {
    }
}

// 从指定 fd 读取一段文本；dup2_test 用它验证 pipe read fd 复制后的读取语义。
static int user_read(int fd, char* buffer, int size) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_READ), "b"(fd), "c"(buffer), "d"(size)
                         : "memory");
    return result;
}

// 打开一个教学版只读文件；这里用来补充验证普通文件 fd 也可以通过 dup2 复制。
static int user_open(const char* path) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_OPEN), "b"(path) : "memory");
    return result;
}

// 向指定 fd 写入一段文本；dup2_test 用它验证 pipe write fd 复制后的写入路径。
static int user_fd_write(int fd, const char* buffer, int size) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_FD_WRITE), "b"(fd), "c"(buffer), "d"(size)
                         : "memory");
    return result;
}

// 关闭一个教学版 fd；这里用来清理 pipe 两端与复制后的 fd。
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

// 输出一个十进制整数，便于观察 fd 编号与 dup2 返回值。
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

// 输出统一错误提示，便于快速定位 dup2 的失败阶段。
static void dup2_test_write_error(const char* message, int code) {
    user_write("dup2_test: ");
    user_write(message);
    if (code != 0) {
        user_write(" (");
        user_write_int(code);
        user_write(")");
    }
    user_write("\n");
}

// 写入期望文本并比较读回结果；一致返回 0，不一致返回负值。
static int dup2_test_compare_text(const char* expected, const char* actual, int size) {
    int index = 0;

    while (index < size) {
        if (expected[index] != actual[index]) {
            return -1;
        }
        index++;
    }

    return 0;
}

// dup2_test 主流程：优先验证 pipe write/read fd 复制到普通 fd，再补充最小错误路径。
void _start(void) {
    int fds[2];
    char buffer[64];
    const char* first_message = "hello dup2\n";
    const char* second_message = "read side\n";
    const char* readme_path = "/readme.txt";
    int result;
    int read_result;
    int eof_result;
    int file_fd;

    user_write("dup2_test: start\n");

    result = user_pipe(fds);
    if (result != 0) {
        dup2_test_write_error("pipe failed", result);
        user_exit(1);
    }

    result = user_dup2(fds[1], 5);
    if (result != 5) {
        dup2_test_write_error("dup write fd failed", result);
        user_exit(1);
    }

    result = user_fd_write(5, first_message, 11);
    if (result != 11) {
        dup2_test_write_error("write via duplicated fd failed", result);
        user_exit(1);
    }

    read_result = user_read(fds[0], buffer, 63);
    if (read_result != 11) {
        dup2_test_write_error("read original pipe fd failed", read_result);
        user_exit(1);
    }
    buffer[read_result] = '\0';
    if (dup2_test_compare_text(first_message, buffer, 11) != 0) {
        dup2_test_write_error("duplicated pipe write data mismatch", 0);
        user_exit(1);
    }
    user_write("dup2_test: write via duplicated pipe write fd ok\n");

    result = user_fd_write(fds[1], second_message, 10);
    if (result != 10) {
        dup2_test_write_error("write original pipe fd failed", result);
        user_exit(1);
    }

    result = user_dup2(fds[0], 6);
    if (result != 6) {
        dup2_test_write_error("dup read fd failed", result);
        user_exit(1);
    }

    read_result = user_read(6, buffer, 63);
    if (read_result != 10) {
        dup2_test_write_error("read via duplicated fd failed", read_result);
        user_exit(1);
    }
    buffer[read_result] = '\0';
    if (dup2_test_compare_text(second_message, buffer, 10) != 0) {
        dup2_test_write_error("duplicated pipe read data mismatch", 0);
        user_exit(1);
    }
    user_write("dup2_test: read via duplicated pipe read fd ok\n");

    result = user_dup2(5, 5);
    if (result != 5) {
        dup2_test_write_error("dup same fd failed", result);
        user_exit(1);
    }

    if (user_dup2(-1, 5) != -1) {
        dup2_test_write_error("invalid oldfd should fail", 0);
        user_exit(1);
    }
    if (user_dup2(5, -1) != -1) {
        dup2_test_write_error("invalid newfd should fail", 0);
        user_exit(1);
    }
    if (user_dup2(5, 99) != -1) {
        dup2_test_write_error("too large newfd should fail", 0);
        user_exit(1);
    }

    eof_result = user_read(6, buffer, 63);
    user_write("dup2_test: eof=");
    user_write_int(eof_result);
    user_write("\n");

    user_close(6);
    user_close(5);
    user_close(fds[0]);
    user_close(fds[1]);

    file_fd = user_open(readme_path);
    if (file_fd < 0) {
        dup2_test_write_error("open readme failed", file_fd);
        user_exit(1);
    }

    result = user_dup2(file_fd, 7);
    if (result != 7) {
        dup2_test_write_error("dup file fd failed", result);
        user_exit(1);
    }

    read_result = user_read(7, buffer, 6);
    if (read_result != 6) {
        dup2_test_write_error("read duplicated file fd failed", read_result);
        user_exit(1);
    }
    if (dup2_test_compare_text("MiniOS", buffer, 6) != 0) {
        dup2_test_write_error("duplicated file fd data mismatch", 0);
        user_exit(1);
    }

    user_close(7);
    user_close(file_fd);
    user_write("dup2_test: ok\n");
    user_exit(0);
}
