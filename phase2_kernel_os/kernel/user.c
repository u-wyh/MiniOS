#include "elf.h"
#include "mm.h"
#include "paging.h"
#include "user.h"

#define USER_STACK_TOP 0x00800000
#define USER_STACK_SIZE 4096

// 汇编入口：构造 iret 返回帧，从 Ring0 切换到 Ring3
extern void enter_user_mode(unsigned int user_entry, unsigned int user_stack_top);

// 防止重复初始化用户空间
static int user_space_ready = 0;
// shell 命令触发的“待进入用户态”请求标志
static int user_enter_pending = 0;
// ELF Loader 返回的用户程序入口
static unsigned int user_entry = 0;
// 用户栈对应的一页物理内存
static unsigned char* user_stack_page = (unsigned char*)0;

// 内存内置的最小 ELF 用户程序：进入后执行 write/getpid/time/exit
static const unsigned char user_elf_image[] = {
    0x7F, 0x45, 0x4C, 0x46, 0x01, 0x01, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x54, 0x00, 0x40, 0x00, 0x34, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x34, 0x00, 0x20, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,

    0x01, 0x00, 0x00, 0x00, 0x54, 0x00, 0x00, 0x00,
    0x54, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x38, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00,

    0xB8, 0x01, 0x00, 0x00, 0x00,
    0xBB, 0x7C, 0x00, 0x40, 0x00,
    0xCD, 0x80,
    0xB8, 0x03, 0x00, 0x00, 0x00,
    0xCD, 0x80,
    0xB8, 0x04, 0x00, 0x00, 0x00,
    0xCD, 0x80,
    0xBB, 0x00, 0x00, 0x00, 0x00,
    0xB8, 0x02, 0x00, 0x00, 0x00,
    0xCD, 0x80,
    0xEB, 0xFE,

    'H', 'e', 'l', 'l', 'o', ' ', 'f', 'r', 'o', 'm', ' ', 'E', 'L', 'F', '\n', 0x00
};

// 初始化最小用户空间：映射用户栈并通过 ELF Loader 准备用户入口
void user_space_init(void) {
    if (user_space_ready != 0) {
        return;
    }

    // 先准备用户栈并建立用户可访问映射
    user_stack_page = (unsigned char*)alloc_page();
    if (user_stack_page == (unsigned char*)0) {
        return;
    }

    map_page(USER_STACK_TOP - USER_STACK_SIZE, (unsigned int)user_stack_page, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    // 从内置内存 ELF 镜像加载用户程序，拿到 e_entry
    user_entry = elf_load(user_elf_image, sizeof(user_elf_image));
    if (user_entry == 0) {
        return;
    }

    user_space_ready = 1;
}

// 由 shell 命令设置一次“进入用户态”请求标志
void user_request_enter(void) {
    user_enter_pending = 1;
}

// 提供给内核主循环轮询，判断是否有待执行的用户态切换请求
int user_has_pending_request(void) {
    return user_enter_pending;
}

// 执行一次 Ring3 切换，入口地址来自 ELF Header 的 e_entry
void user_enter_mode(void) {
    if (user_space_ready == 0) {
        user_space_init();
    }

    if (user_space_ready == 0) {
        return;
    }

    // 清除请求标志后，按 ELF 入口进入用户态
    user_enter_pending = 0;
    enter_user_mode(user_entry, USER_STACK_TOP);
}
