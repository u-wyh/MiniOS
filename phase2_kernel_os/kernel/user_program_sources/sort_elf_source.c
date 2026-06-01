// sort_elf_source.c：最小用户态 sort 程序，从 stdin 读取并按行做字典序升序排序

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_GET_ARGC 9
#define SYS_READ 22

#define SORT_READ_CHUNK 32
#define SORT_BUFFER_SIZE 512
#define SORT_MAX_LINES 64

// 教学版排序行表：记录每一行的起始地址、长度，以及原始输入里是否带换行。
struct sort_line_entry {
    char* start;
    int length;
    int has_newline;
};

// 最小输出 syscall 包装：把以 '\0' 结尾的字符串输出到控制台或重定向目标。
static void user_write(const char* text) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_WRITE), "b"(text) : "memory");
}

// 最小退出 syscall 包装：结束当前用户态 sort 程序。
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

// 从指定 fd 读取一小段文本；sort 当前固定从 fd=0 读取 stdin。
static int user_read(int fd, char* buffer, int size) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_READ), "b"(fd), "c"(buffer), "d"(size)
                         : "memory");
    return result;
}

// 统一输出教学版 sort 错误提示。
static void sort_write_error(const char* message) {
    user_write("sort: ");
    user_write(message);
    user_write("\n");
}

// 输出教学版 sort 用法，提醒当前不支持参数与文件列表。
static void sort_write_usage(void) {
    user_write("Usage: sort\n");
}

// 从 stdin 把全部输入读入固定缓冲区；超出容量时返回失败，避免复杂动态内存管理。
static int sort_read_all_input(char* buffer, int capacity, int* out_size) {
    int current_size = 0;

    if (buffer == (char*)0 || out_size == (int*)0 || capacity <= 0) {
        return -1;
    }

    for (;;) {
        int read_result = user_read(0, &buffer[current_size], capacity - current_size);

        if (read_result < 0) {
            return -2;
        }

        if (read_result == 0) {
            break;
        }

        current_size += read_result;
        if (current_size >= capacity) {
            // 容量耗尽后直接报错退出：当前任务重点是验证数据流，而不是实现外部排序。
            *out_size = current_size;
            return -3;
        }
    }

    *out_size = current_size;
    return 0;
}

// 按 '\n' 把输入缓冲区切分成若干行；最后一行即使没有换行也会保留。
static int sort_split_lines(char* buffer, int size, struct sort_line_entry* lines, int max_lines, int* out_count) {
    int line_count = 0;
    int line_start = 0;
    int index;

    if (buffer == (char*)0 || lines == (struct sort_line_entry*)0 || out_count == (int*)0) {
        return -1;
    }

    for (index = 0; index < size; index++) {
        if (buffer[index] != '\n') {
            continue;
        }

        if (line_count >= max_lines) {
            return -2;
        }

        lines[line_count].start = &buffer[line_start];
        lines[line_count].length = index - line_start;
        lines[line_count].has_newline = 1;
        line_count++;
        line_start = index + 1;
    }

    if (line_start < size) {
        if (line_count >= max_lines) {
            return -2;
        }

        lines[line_count].start = &buffer[line_start];
        lines[line_count].length = size - line_start;
        lines[line_count].has_newline = 0;
        line_count++;
    }

    *out_count = line_count;
    return 0;
}

// 按教学版字典序比较两行：逐字节比较，前缀相同时较短行排在前面。
static int sort_compare_lines(const struct sort_line_entry* left, const struct sort_line_entry* right) {
    int limit;
    int index;

    if (left == (const struct sort_line_entry*)0 || right == (const struct sort_line_entry*)0) {
        return 0;
    }

    limit = left->length;
    if (right->length < limit) {
        limit = right->length;
    }

    for (index = 0; index < limit; index++) {
        unsigned char left_ch = (unsigned char)left->start[index];
        unsigned char right_ch = (unsigned char)right->start[index];

        if (left_ch < right_ch) {
            return -1;
        }
        if (left_ch > right_ch) {
            return 1;
        }
    }

    if (left->length < right->length) {
        return -1;
    }
    if (left->length > right->length) {
        return 1;
    }
    return 0;
}

// 使用最简单的冒泡排序重排行表；当前数据规模很小，优先保持实现直观。
static void sort_sort_lines(struct sort_line_entry* lines, int line_count) {
    int i;
    int j;

    if (lines == (struct sort_line_entry*)0 || line_count <= 1) {
        return;
    }

    for (i = 0; i < line_count - 1; i++) {
        for (j = 0; j < line_count - 1 - i; j++) {
            if (sort_compare_lines(&lines[j], &lines[j + 1]) > 0) {
                struct sort_line_entry temp = lines[j];
                lines[j] = lines[j + 1];
                lines[j + 1] = temp;
            }
        }
    }
}

// 按排序后的顺序输出每一行；当原始输入里有换行时继续保留换行。
static void sort_emit_lines(struct sort_line_entry* lines, int line_count) {
    int index;

    if (lines == (struct sort_line_entry*)0 || line_count <= 0) {
        return;
    }

    for (index = 0; index < line_count; index++) {
        char saved = lines[index].start[lines[index].length];

        lines[index].start[lines[index].length] = '\0';
        user_write(lines[index].start);
        lines[index].start[lines[index].length] = saved;

        if (lines[index].has_newline != 0) {
            user_write("\n");
        }
    }
}

// 用户态 sort 主流程：整段读入 stdin，切分为行，按字典序升序排序后再输出。
void _start(void) {
    char buffer[SORT_BUFFER_SIZE + 1];
    struct sort_line_entry lines[SORT_MAX_LINES];
    int argc;
    int input_size = 0;
    int line_count = 0;
    int read_status;
    int split_status;

    argc = user_get_argc();
    if (argc > 1) {
        sort_write_usage();
        user_exit(1);
    }

    read_status = sort_read_all_input(buffer, SORT_BUFFER_SIZE, &input_size);
    if (read_status == -2) {
        sort_write_error("stdin read failed");
        user_exit(1);
    }
    if (read_status == -3) {
        sort_write_error("input too large");
        user_exit(1);
    }
    if (read_status < 0) {
        sort_write_error("internal error");
        user_exit(1);
    }

    if (input_size == 0) {
        user_exit(0);
    }

    buffer[input_size] = '\0';
    split_status = sort_split_lines(buffer, input_size, lines, SORT_MAX_LINES, &line_count);
    if (split_status == -2) {
        sort_write_error("too many lines");
        user_exit(1);
    }
    if (split_status < 0) {
        sort_write_error("split failed");
        user_exit(1);
    }

    sort_sort_lines(lines, line_count);
    sort_emit_lines(lines, line_count);
    user_exit(0);
}
