// sleep_test_elf_source.c：最小用户态睡眠测试程序，用于观察 SLEEPING 状态与 AGE 增长

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_SLEEP 16
#define SYS_GET_TICKS 19

// 最小输出 syscall 包装。
static void user_write(const char* text) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_WRITE), "b"(text) : "memory");
}

// 最小 sleep syscall 包装。
static int user_sleep(unsigned int ticks) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_SLEEP), "b"(ticks) : "memory");
    return result;
}

// 最小 get_ticks syscall 包装：读取系统启动以来累计 tick 数。
static unsigned int user_get_ticks(void) {
    unsigned int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_GET_TICKS) : "memory");
    return result;
}

// 最小无符号整数输出：便于观察 sleep 前后的 tick 变化。
static void user_write_uint(unsigned int value) {
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

// 保留退出接口，便于后续扩展；当前测试程序默认常驻循环。
static void user_exit(int status) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_EXIT), "b"(status) : "memory");
    for (;;) {
    }
}

// 让程序持续在“输出一行 -> sleep -> 再次运行”之间切换，便于 ps 观察状态变化。
void _start(void) {
    for (;;) {
        user_write("sleep_test before tick=");
        user_write_uint(user_get_ticks());
        user_write("\n");
        if (user_sleep(40) < 0) {
            user_write("sleep_test sleep failed\n");
            user_exit(1);
        }
        user_write("sleep_test after tick=");
        user_write_uint(user_get_ticks());
        user_write("\n");
    }
}
