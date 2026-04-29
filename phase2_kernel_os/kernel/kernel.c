#include "idt.h"
#include "vga.h"

// 内核主函数：初始化显示与中断后，手动触发一次软件中断
void kernel_main(void) {
    clear_screen();
    print_string("MiniOS Kernel Boot Success\n");
    print_string("[OK] GDT LOADED\n");

    // 初始化最小 IDT，建立 int 0x80 处理路径
    idt_init();
    print_string("[OK] IDT LOADED\n");

    // 手动触发软件中断，验证 IDT 与 ISR 是否生效
    __asm__ __volatile__("int $0x80");
    if (idt_was_int80_handled()) {
        print_string("[OK] RETURNED VIA IRET\n");
    } else {
        print_string("[ERR] INT 0x80 NOT HANDLED\n");
    }

    // 当前阶段仅保持内核停在此处运行
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
