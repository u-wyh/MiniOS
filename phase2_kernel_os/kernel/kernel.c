#include "idt.h"
#include "mm.h"
#include "paging.h"
#include "pic.h"
#include "pit.h"
#include "sched.h"
#include "shell.h"
#include "task.h"
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

// 内核主函数：初始化显示、软件中断、PIT 与键盘中断
void kernel_main(void) {
    if (higher_half_active == 0) {
        mm_init();
        paging_init();
        jump_to_higher_half();
    }

    clear_screen();
    print_string("MiniOS Kernel Boot Success\n");

    // 先建立 IDT，再配置 PIC 和 PIT，最后才开启中断
    idt_init();
    pic_remap();
    pit_init(20);
    task_init();
    scheduler_init();

    // 保留 Task4 验证路径，先手动触发一次软件中断
    __asm__ __volatile__("int $0x80");

    // 在中断路径验证完成后打印 Shell 提示符，避免提示符被启动信息打断
    shell_init();
    print_char('\n');

    // 所有中断准备完成后再打开 IF，允许 IRQ0/IRQ1 进入 CPU
    __asm__ __volatile__("sti");

    // 中断开启后立即启动第一个任务，后续切换改由 PIT 自动驱动
    scheduler_start();
}
