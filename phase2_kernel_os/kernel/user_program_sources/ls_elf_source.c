// ls_elf_source.c：最小用户态 ls 程序，通过文件列表 syscall 枚举内置只读文件

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_GET_ARGC 9
#define SYS_FILE_COUNT 24
#define SYS_FILE_INFO 25

#define LS_PATH_MAX_LEN 32

// 最小输出 syscall 包装：把以 '\0' 结尾的字符串输出到控制台。
static void user_write(const char* text) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_WRITE), "b"(text) : "memory");
}

// 最小退出 syscall 包装：结束当前用户态 ls 程序。
static void user_exit(int status) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_EXIT), "b"(status) : "memory");
    for (;;) {
    }
}

// 最小 argc 包装：当前主要用于决定是否提示忽略多余参数。
static int user_get_argc(void) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_GET_ARGC) : "memory");
    return result;
}

// 查询当前内置只读文件数量。
static int user_file_count(void) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_FILE_COUNT) : "memory");
    return result;
}

// 读取指定索引文件的路径，并返回该文件大小。
static int user_file_info(int index, char* buffer, int max_len) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_FILE_INFO), "b"(index), "c"(buffer), "d"(max_len)
                         : "memory");
    return result;
}

// 逐字节输出十进制整数，便于显示文件大小。
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

// 输出一段最小空格填充，让 NAME/SIZE 两列保持基本可读。
static void user_write_padding(int current_len, int target_len) {
    while (current_len < target_len) {
        user_write(" ");
        current_len++;
    }
}

// 统计用户态缓冲区里的路径长度，供输出对齐使用。
static int user_string_len(const char* text) {
    int length = 0;

    while (text[length] != '\0') {
        length++;
    }

    return length;
}

// 统一输出最小错误提示，保持教学版 ls 行为简单稳定。
static void ls_write_error(const char* message, int code) {
    user_write("ls: ");
    user_write(message);
    if (code != 0) {
        user_write(" (");
        user_write_int(code);
        user_write(")");
    }
    user_write("\n");
}

// 用户态 ls 主流程：通过文件列表 syscall 枚举内置只读文件并输出路径/大小。
void _start(void) {
    int count;
    int index;
    char path[LS_PATH_MAX_LEN];

    if (user_get_argc() > 1) {
        user_write("ls: args ignored\n");
    }

    count = user_file_count();
    if (count < 0) {
        ls_write_error("count failed", count);
        user_exit(1);
    }

    user_write("NAME              SIZE\n");
    for (index = 0; index < count; index++) {
        int size = user_file_info(index, path, LS_PATH_MAX_LEN);

        if (size < 0) {
            ls_write_error("info failed", size);
            user_exit(1);
        }

        user_write(path);
        user_write_padding(user_string_len(path), 18);
        user_write_int(size);
        user_write("\n");
    }

    user_exit(0);
}
