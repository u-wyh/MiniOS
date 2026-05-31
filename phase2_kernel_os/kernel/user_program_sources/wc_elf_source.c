// wc_elf_source.c：最小用户态 wc 程序，通过 stdin 统计 bytes / lines / words

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_READ 22

#define WC_READ_CHUNK 32

// 最小输出 syscall 包装：把以 '\0' 结尾的字符串输出到控制台或重定向目标。
static void user_write(const char* text) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_WRITE), "b"(text) : "memory");
}

// 最小退出 syscall 包装：结束当前用户态 wc 程序。
static void user_exit(int status) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_EXIT), "b"(status) : "memory");
    for (;;) {
    }
}

// 最小 read 包装：从指定 fd 读取数据；wc 当前固定从 fd=0 读取 stdin。
static int user_read(int fd, char* buffer, int size) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_READ), "b"(fd), "c"(buffer), "d"(size)
                         : "memory");
    return result;
}

// 输出十进制无符号整数，供 bytes / lines / words 统计结果打印复用。
static void wc_write_uint(unsigned int value) {
    char digits[16];
    int index = 0;

    if (value == 0) {
        user_write("0");
        return;
    }

    while (value > 0) {
        digits[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (index > 0) {
        char one[2];
        index--;
        one[0] = digits[index];
        one[1] = '\0';
        user_write(one);
    }
}

// 判断一个字节是否属于教学版“空白分隔符”，供最小 words 统计复用。
static int wc_is_space(char ch) {
    return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t';
}

// 统一输出教学版 wc 错误提示。
static void wc_write_error(const char* message, int code) {
    char sign[2];

    user_write("wc: ");
    user_write(message);
    if (code != 0) {
        user_write(" (");
        if (code < 0) {
            sign[0] = '-';
            sign[1] = '\0';
            user_write(sign);
            wc_write_uint((unsigned int)(-code));
        } else {
            wc_write_uint((unsigned int)code);
        }
        user_write(")");
    }
    user_write("\n");
}

// 用户态 wc 主流程：循环从 stdin 读取，统计 bytes / lines / words，再把结果输出到 stdout。
void _start(void) {
    char buffer[WC_READ_CHUNK];
    unsigned int bytes = 0;
    unsigned int lines = 0;
    unsigned int words = 0;
    int in_word = 0;

    for (;;) {
        int read_result = user_read(0, buffer, WC_READ_CHUNK);
        int i;

        if (read_result < 0) {
            wc_write_error("stdin read failed", read_result);
            user_exit(1);
        }

        if (read_result == 0) {
            break;
        }

        bytes += (unsigned int)read_result;
        for (i = 0; i < read_result; i++) {
            if (buffer[i] == '\n') {
                lines++;
            }

            if (wc_is_space(buffer[i]) != 0) {
                in_word = 0;
                continue;
            }

            if (in_word == 0) {
                words++;
                in_word = 1;
            }
        }
    }

    user_write("bytes: ");
    wc_write_uint(bytes);
    user_write("\n");
    user_write("lines: ");
    wc_write_uint(lines);
    user_write("\n");
    user_write("words: ");
    wc_write_uint(words);
    user_write("\n");
    user_exit(0);
}
