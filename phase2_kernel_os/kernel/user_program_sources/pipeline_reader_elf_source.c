// pipeline_reader_elf_source.c：最小用户态 reader，从 fd=0 读取并经 fd=1 输出

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_READ 22

// 向当前 stdout 输出一段文本。
static void user_write(const char* text) {
    __asm__ __volatile__(
        "int $0x80"
        :
        : "a"(SYS_WRITE), "b"(text)
        : "memory");
}

// 从指定 fd 读取一段文本。
static int user_read(int fd, char* buf, int size) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_READ), "b"(fd), "c"(buf), "d"(size)
        : "memory");
    return result;
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

// 主流程：从 fd=0 读出 pipe 数据，再写到当前 stdout。
void _start(void) {
    char buffer[128];
    int read_result;

    read_result = user_read(0, buffer, 127);
    if (read_result > 0) {
        buffer[read_result] = '\0';
        user_write("pipeline_reader got:\n");
        user_write(buffer);
    }

    user_write("pipeline_reader: ok\n");
    user_exit(0);
}
