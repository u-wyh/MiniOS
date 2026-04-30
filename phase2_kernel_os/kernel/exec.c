#include "elf.h"
#include "user.h"
#include "vga.h"

#define USER_STACK_TOP 0x00800000

// 汇编入口：通过 iret 切换到用户态并从指定入口开始执行
extern void enter_user_mode(unsigned int user_entry, unsigned int user_stack_top);

// 最小字符串比较，仅用于识别内置程序名
static int exec_str_equal(const char* a, const char* b) {
    int i = 0;

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }

    return a[i] == '\0' && b[i] == '\0';
}

// 按程序名加载并执行用户程序：当前只支持内置 test
void exec(const char* name) {
    const unsigned char* elf_image;
    unsigned int elf_size;
    unsigned int entry;

    if (name == (const char*)0) {
        print_string("exec: empty name\n");
        return;
    }

    if (exec_str_equal(name, "test") == 0) {
        print_string("exec: program not found\n");
        return;
    }

    // 复用现有用户空间初始化，确保用户栈和基础映射已经准备好
    user_space_init();

    elf_image = elf_get_test_image(&elf_size);
    entry = elf_load(elf_image, elf_size);
    if (entry == 0) {
        print_string("exec: elf load failed\n");
        return;
    }

    // 最小 exec 流程：直接进入用户态执行 ELF 入口
    enter_user_mode(entry, USER_STACK_TOP);
}
