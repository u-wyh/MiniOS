// loop_exit_elf_source.c：最小可退出循环程序，用于验证 run/start 后的正常退出与回收

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

// 最小 exit syscall 包装。
static void user_exit(int status) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_EXIT), "b"(status) : "memory");
    for (;;) {
    }
}

// 运行三轮最小循环后主动退出，便于验证 loop 类程序的退出/回收语义。
void _start(void) {
    int round;

    user_write("loop_exit start\n");
    for (round = 0; round < 3; round++) {
        user_write("loop_exit tick\n");
        if (user_sleep(10) < 0) {
            user_write("loop_exit sleep failed\n");
            user_exit(1);
        }
    }

    user_write("loop_exit done\n");
    user_exit(0);
}
