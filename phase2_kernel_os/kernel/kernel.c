#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "vga.h"

// 内核主函数：初始化显示、软件中断与 PIT 定时器中断
void kernel_main(void) {
    clear_screen();
    print_string("MiniOS Kernel Boot Success\n");

    // 先建立 IDT，再配置 PIC 和 PIT，最后才开启中断
    idt_init();
    pic_remap();
    pit_init(20);

    // 保留 Task4 验证路径，先手动触发一次软件中断
    __asm__ __volatile__("int $0x80");

    // 所有中断准备完成后再打开 IF，允许 IRQ0 进入 CPU
    __asm__ __volatile__("sti");

    // 用 hlt 等待下一次定时器中断，避免 CPU 空转
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
