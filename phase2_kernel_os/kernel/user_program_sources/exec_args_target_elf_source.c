// exec_args_target_elf_source.c：打印当前教学版 exec 传递过来的 argc / argv，验证最小参数语义

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_GET_ARGC 9
#define SYS_GET_ARG 10

// 向当前 stdout 输出一段文本。
static void user_write(const char* text) {
    __asm__ __volatile__(
        "int $0x80"
        :
        : "a"(SYS_WRITE), "b"(text)
        : "memory");
}

// 结束当前用户态程序。
static void user_exit(int status) {
    __asm__ __volatile__(
        "int $0x80"
        :
        : "a"(SYS_EXIT), "b"(status)
        : "memory");

    for (;;) {
    }
}

// 读取当前教学版 argc。
static int user_get_argc(void) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_GET_ARGC)
        : "memory");
    return result;
}

// 把指定 argv 复制到用户缓冲区。
static int user_get_arg(int index, char* buffer, int max_len) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_GET_ARG), "b"(index), "c"(buffer), "d"(max_len)
        : "memory");
    return result;
}

// 输出十进制整数，便于打印 argc 和索引。
static void user_write_int(int value) {
    char buf[16];
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
        buf[index++] = (char)('0' + (number % 10));
        number /= 10;
    }

    while (index > 0) {
        char ch[2];
        index--;
        ch[0] = buf[index];
        ch[1] = '\0';
        user_write(ch);
    }
}

// 主流程：逐项打印当前收到的教学版 argc / argv。
void _start(void) {
    int argc;
    int index;
    char buffer[32];

    argc = user_get_argc();
    user_write("exec_args_target: argc = ");
    user_write_int(argc);
    user_write("\n");

    for (index = 0; index < argc; index++) {
        int result = user_get_arg(index, buffer, 32);
        if (result < 0) {
            user_write("exec_args_target: get_arg failed\n");
            user_exit(1);
        }

        user_write("argv[");
        user_write_int(index);
        user_write("] = ");
        user_write(buffer);
        user_write("\n");
    }

    user_write("exec_args_target: ok\n");
    user_exit(0);
}
