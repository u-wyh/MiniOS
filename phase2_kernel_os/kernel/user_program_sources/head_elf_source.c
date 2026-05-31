// head_elf_source.c：最小用户态 head 程序，从 stdin 读取并输出前 N 行

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_GET_ARGC 9
#define SYS_GET_ARG 10
#define SYS_READ 22

#define HEAD_ARG_MAX_LEN 32
#define HEAD_READ_CHUNK 32

// 最小输出 syscall 包装：把以 '\0' 结尾的字符串输出到控制台或重定向目标。
static void user_write(const char* text) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_WRITE), "b"(text) : "memory");
}

// 最小退出 syscall 包装：结束当前用户态 head 程序。
static void user_exit(int status) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_EXIT), "b"(status) : "memory");
    for (;;) {
    }
}

// 返回当前程序保存的教学版参数数量。
static int user_get_argc(void) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_GET_ARGC) : "memory");
    return result;
}

// 把指定参数复制到用户缓冲区，成功返回长度。
static int user_get_arg(int index, char* buffer, int max_len) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_GET_ARG), "b"(index), "c"(buffer), "d"(max_len)
                         : "memory");
    return result;
}

// 从指定 fd 读取一小段文本；head 当前固定从 fd=0 读取 stdin。
static int user_read(int fd, char* buffer, int size) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_READ), "b"(fd), "c"(buffer), "d"(size)
                         : "memory");
    return result;
}

// 统一输出教学版 head 错误提示。
static void head_write_error(const char* message) {
    user_write("head: ");
    user_write(message);
    user_write("\n");
}

// 判断两个以 '\0' 结尾的字符串是否完全相等，供参数解析复用。
static int head_string_equals(const char* left, const char* right) {
    if (left == (const char*)0 || right == (const char*)0) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        if (*left != *right) {
            return 0;
        }
        left++;
        right++;
    }

    return (*left == '\0' && *right == '\0') ? 1 : 0;
}

// 解析十进制非负整数；遇到空串、负号或非数字时返回失败。
static int head_parse_non_negative_int(const char* text, int* out_value) {
    int value = 0;
    int saw_digit = 0;

    if (text == (const char*)0 || out_value == (int*)0) {
        return -1;
    }

    while (*text != '\0') {
        char ch = *text;

        if (ch < '0' || ch > '9') {
            return -1;
        }

        saw_digit = 1;
        value = value * 10 + (int)(ch - '0');
        text++;
    }

    if (saw_digit == 0) {
        return -1;
    }

    *out_value = value;
    return 0;
}

// 解析 head 当前支持的最小参数集合：默认 10 行，或 "-n N"。
static int head_parse_limit(int argc, int* out_limit) {
    char option[HEAD_ARG_MAX_LEN];
    char count_text[HEAD_ARG_MAX_LEN];

    if (out_limit == (int*)0) {
        return -1;
    }

    *out_limit = 10;
    if (argc <= 1) {
        return 0;
    }

    if (argc != 3) {
        return -1;
    }

    if (user_get_arg(1, option, HEAD_ARG_MAX_LEN) <= 0) {
        return -1;
    }

    if (head_string_equals(option, "-n") == 0) {
        return -1;
    }

    if (user_get_arg(2, count_text, HEAD_ARG_MAX_LEN) <= 0) {
        return -1;
    }

    return head_parse_non_negative_int(count_text, out_limit);
}

// 输出教学版 head Usage，提醒当前只支持默认 10 行或 "-n N"。
static void head_write_usage(void) {
    user_write("Usage: head [-n N]\n");
}

// 用户态 head 主流程：从 stdin 循环读取，输出前 N 行后立即停止。
void _start(void) {
    char buffer[HEAD_READ_CHUNK + 1];
    int argc;
    int limit;
    int lines_seen = 0;

    argc = user_get_argc();
    if (head_parse_limit(argc, &limit) < 0) {
        head_write_usage();
        user_exit(1);
    }

    // 当目标行数为 0 时，直接正常退出，不再读取任何输入。
    if (limit == 0) {
        user_exit(0);
    }

    for (;;) {
        int read_result = user_read(0, buffer, HEAD_READ_CHUNK);
        int output_len = read_result;
        int stop_after_chunk = 0;
        int i;

        if (read_result < 0) {
            head_write_error("stdin read failed");
            user_exit(1);
        }

        if (read_result == 0) {
            break;
        }

        // 逐字符统计换行；一旦达到目标行数，只输出到该换行为止。
        for (i = 0; i < read_result; i++) {
            if (buffer[i] != '\n') {
                continue;
            }

            lines_seen++;
            if (lines_seen >= limit) {
                output_len = i + 1;
                stop_after_chunk = 1;
                break;
            }
        }

        buffer[output_len] = '\0';
        if (output_len > 0) {
            user_write(buffer);
        }

        if (stop_after_chunk != 0) {
            break;
        }
    }

    user_exit(0);
}
