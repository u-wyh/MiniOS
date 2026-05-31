// grep_elf_source.c：最小用户态 grep 程序，从 stdin 读取并输出包含关键字的整行

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_GET_ARGC 9
#define SYS_GET_ARG 10
#define SYS_READ 22

#define GREP_ARG_MAX_LEN 32
#define GREP_READ_CHUNK 32
#define GREP_LINE_MAX 128

// 最小输出 syscall 包装：把以 '\0' 结尾的字符串输出到控制台或重定向目标。
static void user_write(const char* text) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_WRITE), "b"(text) : "memory");
}

// 最小退出 syscall 包装：结束当前用户态 grep 程序。
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

// 最小 read 包装：grep 当前固定从 fd=0 读取 stdin。
static int user_read(int fd, char* buffer, int size) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_READ), "b"(fd), "c"(buffer), "d"(size)
                         : "memory");
    return result;
}

// 统一输出教学版 grep 错误提示。
static void grep_write_error(const char* message) {
    user_write("grep: ");
    user_write(message);
    user_write("\n");
}

// 把 ASCII 字母折叠成小写，供教学版大小写无关匹配复用。
static char grep_ascii_lower(char ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return (char)(ch - 'A' + 'a');
    }
    return ch;
}

// 输出教学版 grep Usage，提醒当前只支持 grep <keyword> 并从 stdin 读取。
static void grep_write_usage(void) {
    user_write("Usage: grep <keyword>\n");
}

// 判断 line 是否按字节包含 keyword；当前保持教学版 ASCII 大小写无关匹配，不支持正则。
static int grep_contains(const char* line, int line_len, const char* keyword, int keyword_len) {
    int start;
    int index;

    if (keyword_len <= 0 || line_len < keyword_len) {
        return 0;
    }

    for (start = 0; start <= line_len - keyword_len; start++) {
        for (index = 0; index < keyword_len; index++) {
            if (grep_ascii_lower(line[start + index]) != grep_ascii_lower(keyword[index])) {
                break;
            }
        }

        if (index == keyword_len) {
            return 1;
        }
    }

    return 0;
}

// 把当前行输出到 stdout；当原始输入带换行时保留换行。
static void grep_emit_line(char* line, int line_len, int has_newline) {
    char saved;

    saved = line[line_len];
    line[line_len] = '\0';
    user_write(line);
    line[line_len] = saved;

    if (has_newline != 0) {
        user_write("\n");
    }
}

// 用户态 grep 主流程：从 stdin 按行读取，输出包含关键字的整行。
void _start(void) {
    char keyword[GREP_ARG_MAX_LEN];
    char read_buffer[GREP_READ_CHUNK];
    char line_buffer[GREP_LINE_MAX + 1];
    int argc;
    int keyword_len;
    int line_len = 0;
    int dropping_long_line = 0;

    argc = user_get_argc();
    if (argc < 2) {
        grep_write_usage();
        user_exit(1);
    }

    keyword_len = user_get_arg(1, keyword, GREP_ARG_MAX_LEN);
    if (keyword_len <= 0) {
        grep_write_usage();
        user_exit(1);
    }

    for (;;) {
        int read_result = user_read(0, read_buffer, GREP_READ_CHUNK);
        int i;

        if (read_result < 0) {
            grep_write_error("stdin read failed");
            user_exit(1);
        }

        if (read_result == 0) {
            break;
        }

        for (i = 0; i < read_result; i++) {
            char ch = read_buffer[i];

            if (dropping_long_line != 0) {
                if (ch == '\n') {
                    dropping_long_line = 0;
                    line_len = 0;
                }
                continue;
            }

            if (ch == '\n') {
                if (grep_contains(line_buffer, line_len, keyword, keyword_len) != 0) {
                    grep_emit_line(line_buffer, line_len, 1);
                }
                line_len = 0;
                continue;
            }

            if (line_len >= GREP_LINE_MAX) {
                grep_write_error("line too long");
                dropping_long_line = 1;
                line_len = 0;
                continue;
            }

            line_buffer[line_len++] = ch;
            line_buffer[line_len] = '\0';
        }
    }

    if (dropping_long_line == 0 && line_len > 0) {
        if (grep_contains(line_buffer, line_len, keyword, keyword_len) != 0) {
            grep_emit_line(line_buffer, line_len, 0);
        }
    }

    user_exit(0);
}
