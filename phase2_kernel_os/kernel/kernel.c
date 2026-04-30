#include "idt.h"
#include "mm.h"
#include "paging.h"
#include "vga.h"

// 前向声明内核主入口，便于在开启分页后显式跳转到高地址别名执行
void kernel_main(void);
// 汇编入口：通过构造 iretd 栈帧，把 CPU 从 Ring0 主动切到 Ring3
extern void enter_user_mode(unsigned int user_entry, unsigned int user_stack_top);

// 记录分页切换后内核是否已经从低地址别名迁移到高地址别名执行
static int higher_half_active = 0;
// 用户态最小实验栈：本轮只有一个用户任务，因此直接静态预留 4KB
static unsigned char user_stack[4096];

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

// 最小用户态入口：进入 Ring3 后主动执行一次 int 0x80，验证系统调用返回内核路径
static void user_mode_main(void) {
    __asm__ __volatile__("int $0x80");

    // 如果系统调用未来选择返回用户态，这里保持一个简单死循环，避免执行落空
    for (;;) {
        __asm__ __volatile__("jmp .");
    }
}

// 内核主函数：初始化分页和 IDT，然后主动降权进入一次最小用户态执行
void kernel_main(void) {
    unsigned int user_entry;
    unsigned int user_stack_top;

    if (higher_half_active == 0) {
        mm_init();
        paging_init();
        jump_to_higher_half();
    }

    clear_screen();
    print_string("MiniOS Kernel Boot Success\n");

    // 本轮先建立 IDT，再通过 iret 主动切到用户态；不引入 PIT/键盘等额外干扰
    idt_init();
    print_string("Switching to Ring3 user mode...\n");

    // 当前内核已经在高地址别名执行，因此用户入口也同步切到高地址，避免跳回低地址别名
    user_entry = paging_get_kernel_virtual_base() + (unsigned int)&user_mode_main;
    user_stack_top = (unsigned int)(user_stack + sizeof(user_stack));
    enter_user_mode(user_entry, user_stack_top);

    // 如果用户态入口意外返回到这里，说明切换链路出现异常，直接停机观察
    for (;;) {
        __asm__ __volatile__("cli");
        __asm__ __volatile__("hlt");
    }
}
