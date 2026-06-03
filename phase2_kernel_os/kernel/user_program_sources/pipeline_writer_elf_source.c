// pipeline_writer_elf_source.c：最小用户态 writer，专门往 fd=1 输出固定多行文本

#define SYS_WRITE 1
#define SYS_EXIT 2

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

// 主流程：只通过 fd=1 输出固定多行，供 pipeline_demo 与 pipeline_args_demo 复用。
void _start(void) {
    user_write("MiniOS line one\n");
    user_write("normal line two\n");
    user_write("MiniOS line three\n");
    user_write("tail line four\n");
    user_exit(0);
}
