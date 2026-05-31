// tail_elf_source.c：最小用户态 tail 程序，从 stdin 读取并输出最后 N 行

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_GET_ARGC 9
#define SYS_GET_ARG 10
#define SYS_READ 22

#define TAIL_ARG_MAX_LEN 32
#define TAIL_READ_CHUNK 32
#define TAIL_BUFFER_SIZE 512

// 最小输出 syscall 包装：把以 '\0' 结尾的字符串输出到控制台或重定向目标。
static void user_write(const char* text) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_WRITE), "b"(text) : "memory");
}

// 最小退出 syscall 包装：结束当前用户态 tail 程序。
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

// 从指定 fd 读取一小段文本；tail 当前固定从 fd=0 读取 stdin。
static int user_read(int fd, char* buffer, int size) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_READ), "b"(fd), "c"(buffer), "d"(size)
                         : "memory");
    return result;
}

// 统一输出教学版 tail 错误提示。
static void tail_write_error(const char* message) {
    user_write("tail: ");
    user_write(message);
    user_write("\n");
}

// 判断两个以 '\0' 结尾的字符串是否完全相等，供参数解析复用。
static int tail_string_equals(const char* left, const char* right) {
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
static int tail_parse_non_negative_int(const char* text, int* out_value) {
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

// 解析 tail 当前支持的最小参数集合：默认 10 行，或 "-n N"。
static int tail_parse_limit(int argc, int* out_limit) {
    char option[TAIL_ARG_MAX_LEN];
    char count_text[TAIL_ARG_MAX_LEN];

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

    if (user_get_arg(1, option, TAIL_ARG_MAX_LEN) <= 0) {
        return -1;
    }

    if (tail_string_equals(option, "-n") == 0) {
        return -1;
    }

    if (user_get_arg(2, count_text, TAIL_ARG_MAX_LEN) <= 0) {
        return -1;
    }

    return tail_parse_non_negative_int(count_text, out_limit);
}

// 输出教学版 tail Usage，提醒当前只支持默认 10 行或 "-n N"。
static void tail_write_usage(void) {
    user_write("Usage: tail [-n N]\n");
}

// 把新读取的一段文本追加到固定窗口末尾；容量不足时丢弃最旧字节，保留“最后一段输入”。
static void tail_append_chunk(char* window, int* current_size, const char* chunk, int chunk_size, int* out_truncated) {
    int keep_chunk_size = chunk_size;
    int drop_count;
    int i;

    if (window == (char*)0 || current_size == (int*)0 || chunk == (const char*)0 || out_truncated == (int*)0) {
        return;
    }

    if (chunk_size <= 0) {
        return;
    }

    if (keep_chunk_size > TAIL_BUFFER_SIZE) {
        chunk += keep_chunk_size - TAIL_BUFFER_SIZE;
        keep_chunk_size = TAIL_BUFFER_SIZE;
        *current_size = 0;
        *out_truncated = 1;
    }

    if ((*current_size + keep_chunk_size) > TAIL_BUFFER_SIZE) {
        drop_count = *current_size + keep_chunk_size - TAIL_BUFFER_SIZE;

        for (i = 0; i < (*current_size - drop_count); i++) {
            window[i] = window[i + drop_count];
        }

        *current_size -= drop_count;
        *out_truncated = 1;
    }

    for (i = 0; i < keep_chunk_size; i++) {
        window[*current_size + i] = chunk[i];
    }
    *current_size += keep_chunk_size;
}

// 在固定窗口中倒序查找“最后 N 行”的起始位置；若行数不足 N，则返回 0 表示输出全部。
static int tail_find_start_offset(const char* window, int size, int limit) {
    int effective_end = size;
    int lines_found = 0;
    int index;

    if (window == (const char*)0 || size <= 0 || limit <= 0) {
        return size;
    }

    // 若输入以换行结尾，不把最后这个换行当成“空尾行”单独计数。
    if (effective_end > 0 && window[effective_end - 1] == '\n') {
        effective_end--;
    }

    for (index = effective_end - 1; index >= 0; index--) {
        if (window[index] != '\n') {
            continue;
        }

        lines_found++;
        if (lines_found >= limit) {
            return index + 1;
        }
    }

    return 0;
}

// 用户态 tail 主流程：先把 stdin 读入固定窗口，再输出窗口中的最后 N 行。
void _start(void) {
    char read_buffer[TAIL_READ_CHUNK];
    char window[TAIL_BUFFER_SIZE + 1];
    int argc;
    int limit;
    int window_size = 0;
    int start_offset;

    argc = user_get_argc();
    if (tail_parse_limit(argc, &limit) < 0) {
        tail_write_usage();
        user_exit(1);
    }

    // 当目标行数为 0 时，直接正常退出，不再读取任何输入。
    if (limit == 0) {
        user_exit(0);
    }

    for (;;) {
        int read_result = user_read(0, read_buffer, TAIL_READ_CHUNK);

        if (read_result < 0) {
            tail_write_error("stdin read failed");
            user_exit(1);
        }

        if (read_result == 0) {
            break;
        }

        {
            int truncated = 0;
            tail_append_chunk(window, &window_size, read_buffer, read_result, &truncated);
        }
    }

    if (window_size == 0) {
        user_exit(0);
    }

    window[window_size] = '\0';
    start_offset = tail_find_start_offset(window, window_size, limit);
    if (start_offset < window_size) {
        user_write(&window[start_offset]);
    }

    user_exit(0);
}
