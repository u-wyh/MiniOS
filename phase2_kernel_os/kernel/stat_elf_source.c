// stat_elf_source.c：最小用户态 stat 程序，通过 SYS_STAT 查询只读文件元信息

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_GET_ARGC 9
#define SYS_GET_ARG 10
#define SYS_STAT 26

#define STAT_ARG_MAX_LEN 32
#define MINIOS_FILE_TYPE_READONLY_TEXT 1

// 教学版 stat 结构：与内核共享最小 size/type 语义。
struct minios_stat {
    unsigned int size;
    unsigned int type;
};

// 最小输出 syscall 包装：把字符串输出到控制台。
static void user_write(const char* text) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_WRITE), "b"(text) : "memory");
}

// 最小退出 syscall 包装：结束当前用户态 stat 程序。
static void user_exit(int status) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_EXIT), "b"(status) : "memory");
    for (;;) {
    }
}

// 读取当前教学版 argc。
static int user_get_argc(void) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_GET_ARGC) : "memory");
    return result;
}

// 读取一项教学版 argv 到用户缓冲区。
static int user_get_arg(int index, char* buffer, int max_len) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_GET_ARG), "b"(index), "c"(buffer), "d"(max_len)
                         : "memory");
    return result;
}

// 查询指定路径的教学版文件元信息。
static int user_stat(const char* path, struct minios_stat* stat_buf) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_STAT), "b"(path), "c"(stat_buf)
                         : "memory");
    return result;
}

// 输出十进制整数，便于打印文件大小和错误码。
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

// 把当前教学版 type 转成稳定文案；未知值统一返回 unknown。
static const char* stat_type_name(unsigned int type) {
    if (type == MINIOS_FILE_TYPE_READONLY_TEXT) {
        return "readonly-file";
    }

    return "unknown";
}

// 统一输出最小错误提示，保持用户态 stat 风格简单一致。
static void stat_write_error(const char* message, int code) {
    user_write("stat: ");
    user_write(message);
    if (code != 0) {
        user_write(" (");
        user_write_int(code);
        user_write(")");
    }
    user_write("\n");
}

// 用户态 stat 主流程：通过 path 查询 size/type，再直接输出 Name/Size/Type。
void _start(void) {
    int argc;
    char path[STAT_ARG_MAX_LEN];
    struct minios_stat st;
    int result;

    argc = user_get_argc();
    if (argc < 2) {
        user_write("Usage: stat <file>\n");
        user_exit(1);
    }

    if (user_get_arg(1, path, STAT_ARG_MAX_LEN) < 0) {
        stat_write_error("invalid path", 0);
        user_exit(1);
    }

    result = user_stat(path, &st);
    if (result < 0) {
        stat_write_error("stat failed", result);
        user_exit(1);
    }

    user_write("Name: ");
    user_write(path);
    user_write("\n");
    user_write("Size: ");
    user_write_int((int)st.size);
    user_write("\n");
    user_write("Type: ");
    user_write(stat_type_name(st.type));
    user_write("\n");

    user_exit(0);
}
