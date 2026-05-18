// cat_elf_source.c：最小用户态 cat 程序，通过 open/read/close syscall 读取内置只读文件

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_GET_ARGC 9
#define SYS_GET_ARG 10
#define SYS_OPEN 21
#define SYS_READ 22
#define SYS_CLOSE 23

#define CAT_ARG_MAX_LEN 32
#define CAT_READ_CHUNK 32

// 最小输出 syscall 包装：把以 '\0' 结尾的字符串交给内核控制台输出。
static void user_write(const char* text) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_WRITE), "b"(text) : "memory");
}

// 最小退出 syscall 包装：结束当前用户态 cat 程序。
static void user_exit(int status) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_EXIT), "b"(status) : "memory");
    for (;;) {
    }
}

// 最小读取 argc 包装：返回当前程序保存的教学版参数数量。
static int user_get_argc(void) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_GET_ARGC) : "memory");
    return result;
}

// 最小读取 argv 包装：把指定参数复制到用户缓冲区，成功返回长度。
static int user_get_arg(int index, char* buffer, int max_len) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_GET_ARG), "b"(index), "c"(buffer), "d"(max_len)
                         : "memory");
    return result;
}

// 最小 open 包装：按路径打开只读内置文件，成功返回 fd。
static int user_open(const char* path) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_OPEN), "b"(path) : "memory");
    return result;
}

// 最小 read 包装：从 fd 当前 offset 读取数据到用户缓冲区。
static int user_read(int fd, char* buffer, int size) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_READ), "b"(fd), "c"(buffer), "d"(size)
                         : "memory");
    return result;
}

// 最小 close 包装：关闭 fd，释放当前进程里的表项。
static int user_close(int fd) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_CLOSE), "b"(fd) : "memory");
    return result;
}

// 逐字节打印十进制整数，便于在错误提示里附带简单数值。
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

// 统一输出教学版错误前缀，保持用户态 cat 的报错风格简单稳定。
static void cat_write_error(const char* message, int code) {
    user_write("cat: ");
    user_write(message);
    if (code != 0) {
        user_write(" (");
        user_write_int(code);
        user_write(")");
    }
    user_write("\n");
}

// 用户态 cat 主流程：通过 get_arg 获取路径，再经 open/read/close 读取内置只读文件。
void _start(void) {
    int argc;
    int fd;
    char path[CAT_ARG_MAX_LEN];
    char buffer[CAT_READ_CHUNK + 1];

    argc = user_get_argc();
    if (argc < 2) {
        user_write("Usage: cat <file>\n");
        user_exit(1);
    }

    if (user_get_arg(1, path, CAT_ARG_MAX_LEN) < 0) {
        cat_write_error("invalid path", 0);
        user_exit(1);
    }

    fd = user_open(path);
    if (fd < 0) {
        cat_write_error("open failed", fd);
        user_exit(1);
    }

    for (;;) {
        int read_result = user_read(fd, buffer, CAT_READ_CHUNK);

        if (read_result < 0) {
            user_close(fd);
            cat_write_error("read failed", read_result);
            user_exit(1);
        }

        if (read_result == 0) {
            break;
        }

        buffer[read_result] = '\0';
        user_write(buffer);
    }

    if (user_close(fd) < 0) {
        cat_write_error("close failed", 0);
        user_exit(1);
    }

    user_exit(0);
}
