// sleep_test_elf_source.c：最小用户态睡眠测试程序，用于观察 SLEEPING 状态与 AGE 增长

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_SLEEP 16

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

// 保留退出接口，便于后续扩展；当前测试程序默认常驻循环。
static void user_exit(int status) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_EXIT), "b"(status) : "memory");
    for (;;) {
    }
}

// 让程序持续在“输出一行 -> sleep -> 再次运行”之间切换，便于 ps 观察状态变化。
void _start(void) {
    for (;;) {
        user_write("sleep_test tick\n");
        if (user_sleep(40) < 0) {
            user_write("sleep_test sleep failed\n");
            user_exit(1);
        }
    }
}
