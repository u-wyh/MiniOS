// exec_fd_writer_elf_source.c：最小用户态 writer，验证 exec 后 fd=1 仍然可用

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

// 主流程：只往 fd=1 写一段固定文本，供 exec_fd_test 验证 exec 后 stdout 保留。
void _start(void) {
    user_write("message from exec writer\n");
    user_exit(0);
}
