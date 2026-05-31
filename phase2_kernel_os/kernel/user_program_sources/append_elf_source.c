// append_elf_source.c：最小用户态 append 程序，通过 syscall 把文本追加到 RAMFS 文件末尾

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_GET_ARGC 9
#define SYS_GET_ARG 10
#define SYS_APPEND_FILE 32

#define APPEND_PATH_MAX_LEN 32
#define APPEND_ARG_MAX_LEN 32
#define APPEND_TEXT_MAX_LEN 65

// 最小输出 syscall 包装：把以 '\0' 结尾的字符串输出到控制台。
static void user_write(const char* text) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_WRITE), "b"(text) : "memory");
}

// 最小退出 syscall 包装：结束当前用户态 append 程序。
static void user_exit(int status) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_EXIT), "b"(status) : "memory");
    for (;;) {
    }
}

// 返回当前教学版 argc。
static int user_get_argc(void) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_GET_ARGC) : "memory");
    return result;
}

// 把指定 argv 复制到用户缓冲区，成功返回字符串长度。
static int user_get_arg(int index, char* buffer, int max_len) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_GET_ARG), "b"(index), "c"(buffer), "d"(max_len)
                         : "memory");
    return result;
}

// 向 RAMFS 文件末尾追加一段文本，成功返回追加字节数。
static int user_append_file(const char* path, const char* buffer, int size) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_APPEND_FILE), "b"(path), "c"(buffer), "d"(size)
                         : "memory");
    return result;
}

// 统计字符串长度，供拼接文本与追加长度计算复用。
static int user_string_len(const char* text) {
    int length = 0;

    while (text[length] != '\0') {
        length++;
    }

    return length;
}

// 逐字节输出十进制整数，便于打印错误码。
static void user_write_int(int value) {
    char digits[16];
    int index = 0;
    unsigned int current;

    if (value == 0) {
        user_write("0");
        return;
    }

    if (value < 0) {
        user_write("-");
        current = (unsigned int)(-value);
    } else {
        current = (unsigned int)value;
    }

    while (current > 0) {
        digits[index++] = (char)('0' + (current % 10));
        current /= 10;
    }

    while (index > 0) {
        char one[2];
        index--;
        one[0] = digits[index];
        one[1] = '\0';
        user_write(one);
    }
}

// 统一输出教学版错误提示，让用户态 append 报错风格保持简单稳定。
static void append_error(const char* message, int code) {
    user_write("append: ");
    user_write(message);
    if (code != 0) {
        user_write(" (");
        user_write_int(code);
        user_write(")");
    }
    user_write("\n");
}

// 把 argv[start..argc-1] 用单个空格拼接成一段文本，供教学版 append 最小支持多词内容。
static int append_join_text(int argc, int start, char* out_text, int max_len) {
    int arg_index;
    int out_index = 0;

    if (out_text == (char*)0 || max_len <= 0) {
        return -1;
    }

    out_text[0] = '\0';
    for (arg_index = start; arg_index < argc; arg_index++) {
        char one_arg[APPEND_ARG_MAX_LEN];
        int arg_len;
        int i;

        arg_len = user_get_arg(arg_index, one_arg, APPEND_ARG_MAX_LEN);
        if (arg_len < 0) {
            return -2;
        }

        if (arg_index > start) {
            if (out_index >= max_len - 1) {
                return -3;
            }
            out_text[out_index++] = ' ';
        }

        for (i = 0; i < arg_len; i++) {
            if (out_index >= max_len - 1) {
                return -3;
            }
            out_text[out_index++] = one_arg[i];
        }
    }

    out_text[out_index] = '\0';
    return out_index;
}

// 用户态 append 主流程：读取 path/text 参数，经 SYS_APPEND_FILE 把文本追加到 RAMFS 文件末尾。
void _start(void) {
    int argc;
    char path[APPEND_PATH_MAX_LEN];
    char text[APPEND_TEXT_MAX_LEN];
    int text_len;
    int append_result;

    argc = user_get_argc();
    if (argc < 3) {
        user_write("Usage: append <file> <text>\n");
        user_exit(1);
    }

    if (user_get_arg(1, path, APPEND_PATH_MAX_LEN) < 0) {
        append_error("invalid path", 0);
        user_exit(1);
    }

    text_len = append_join_text(argc, 2, text, APPEND_TEXT_MAX_LEN);
    if (text_len < 0) {
        append_error("text too long", 0);
        user_exit(1);
    }

    append_result = user_append_file(path, text, user_string_len(text));
    if (append_result < 0 || append_result != user_string_len(text)) {
        append_error("append failed", append_result);
        user_exit(1);
    }

    user_exit(0);
}
