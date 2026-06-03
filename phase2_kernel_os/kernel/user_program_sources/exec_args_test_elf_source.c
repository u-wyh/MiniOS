// exec_args_test_elf_source.c：最小用户态 exec 参数测试程序，验证 exec 后目标程序能收到教学版 argc / argv

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_EXEC_ARGS 11

#define PROGRAM_EXEC_ARGS_TARGET 34

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

// 调用当前教学版 exec_args。
static int user_exec_args(int program_id, int argc, const char* const* argv) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_EXEC_ARGS), "b"(program_id), "c"(argc), "d"(argv)
        : "memory");
    return result;
}

// 输出十进制整数，便于错误提示。
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

// 主流程：构造最小 argv 并 exec 到目标程序。
void _start(void) {
    static const char* const target_argv[] = {
        "exec_args_target",
        "hello",
        "MiniOS"
    };
    int exec_result;

    user_write("exec_args_test: start\n");
    exec_result = user_exec_args(PROGRAM_EXEC_ARGS_TARGET, 3, target_argv);
    user_write("exec_args_test: exec failed (");
    user_write_int(exec_result);
    user_write(")\n");
    user_exit(1);
}
