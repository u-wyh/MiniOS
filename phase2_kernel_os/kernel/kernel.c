#include "vga.h"

// 内核主函数：通过 VGA 模块清屏并输出启动信息
void kernel_main(void) {
    clear_screen();
    print_string("MiniOS Kernel Boot Success");

    // 当前阶段仅保持内核停在此处运行
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
