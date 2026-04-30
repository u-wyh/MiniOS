#include "idt.h"
#include "mm.h"
#include "paging.h"
#include "pic.h"
#include "pit.h"
#include "shell.h"
#include "user.h"
#include "vga.h"

// 前向声明内核主入口，便于在开启分页后显式跳转到高地址别名执行
void kernel_main(void);

// 记录分页切换后内核是否已经从低地址别名迁移到高地址别名执行
static int higher_half_active = 0;

// 开启分页后直接跳到高地址别名版 kernel_main，确保后续执行流真正进入高地址空间
static void jump_to_higher_half(void) {
    unsigned int high_base = paging_get_kernel_virtual_base();
    unsigned int high_address = high_base + (unsigned int)&kernel_main;

    if (higher_half_active != 0) {
        return;
    }

    higher_half_active = 1;
    __asm__ __volatile__("jmp *%0" : : "r"(high_address));
}

// 内核主函数：恢复 shell / PIT / 键盘环境，并允许通过 user 命令触发一次 Ring3 测试
void kernel_main(void) {
    if (higher_half_active == 0) {
        mm_init();
        paging_init();
        jump_to_higher_half();
    }

    clear_screen();
    print_string("MiniOS Kernel Boot Success\n");

    // 恢复中断与 shell 基础环境，再额外准备用户空间页映射
    idt_init();
    pic_remap();
    pit_init(20);
    user_space_init();
    shell_init();

    __asm__ __volatile__("sti");

    // 平时维持原有 hlt 等待中断；只有收到 user 命令请求时，才真正进入一次 Ring3 测试
    for (;;) {
        if (user_has_pending_request() != 0) {
            user_enter_mode();
        }

        __asm__ __volatile__("hlt");
    }
}
