#include "mm.h"
#include "paging.h"
#include "user.h"

// 用户代码放在 4MB 处，避开低端内核身份映射区域，形成清晰的用户空间入口
#define USER_CODE_VA 0x00400000
// 用户栈顶放在 8MB 处，栈页实际映射在其下方 4KB
#define USER_STACK_TOP 0x00800000
#define USER_STACK_SIZE 4096

// 汇编入口：通过构造 iretd 返回帧，把 CPU 从 Ring0 切到 Ring3
extern void enter_user_mode(unsigned int user_entry, unsigned int user_stack_top);

// 记录用户空间是否已经准备完成，避免重复分配用户页
static int user_space_ready = 0;
// 记录 shell 是否已经请求执行一次用户态测试
static int user_enter_pending = 0;
// 保存用户代码页和用户栈页的物理地址，便于后续调试和文档说明
static unsigned char* user_code_page = (unsigned char*)0;
static unsigned char* user_stack_page = (unsigned char*)0;

// 这是最小用户态测试程序的机器码：
// 1) eax=1 -> int 0x80：让内核打印 "user mode running"
// 2) eax=2 -> int 0x80：让内核打印 "syscall from user mode"
// 3) 跳回自身，防止未来系统调用返回用户态后执行落空
static const unsigned char user_program[] = {
    0xB8, 0x01, 0x00, 0x00, 0x00,
    0xCD, 0x80,
    0xB8, 0x02, 0x00, 0x00, 0x00,
    0xCD, 0x80,
    0xEB, 0xFE
};

// 把内置测试程序拷贝到用户代码页；当前仍是教学验证阶段，没有 ELF loader
static void user_copy_program(void) {
    unsigned int i;

    if (user_code_page == (unsigned char*)0) {
        return;
    }

    for (i = 0; i < USER_STACK_SIZE; i++) {
        user_code_page[i] = 0;
    }

    for (i = 0; i < sizeof(user_program); i++) {
        user_code_page[i] = user_program[i];
    }
}

// 初始化用户空间：分配用户代码页和用户栈页，并建立带 PAGE_USER 的最小映射
void user_space_init(void) {
    if (user_space_ready != 0) {
        return;
    }

    user_code_page = (unsigned char*)alloc_page();
    user_stack_page = (unsigned char*)alloc_page();
    if (user_code_page == (unsigned char*)0 || user_stack_page == (unsigned char*)0) {
        return;
    }

    user_copy_program();
    map_page(USER_CODE_VA, (unsigned int)user_code_page, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    map_page(USER_STACK_TOP - USER_STACK_SIZE, (unsigned int)user_stack_page, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    user_space_ready = 1;
}

// shell 命令只负责提出请求，避免在键盘中断上下文里直接做 Ring3 切换
void user_request_enter(void) {
    user_enter_pending = 1;
}

// 给内核主循环查询是否需要执行用户态测试
int user_has_pending_request(void) {
    return user_enter_pending;
}

// 使用已经规划好的 USER_CODE_VA 和 USER_STACK_TOP 进入 Ring3
void user_enter_mode(void) {
    if (user_space_ready == 0) {
        user_space_init();
    }

    if (user_space_ready == 0) {
        return;
    }

    user_enter_pending = 0;
    enter_user_mode(USER_CODE_VA, USER_STACK_TOP);
}
