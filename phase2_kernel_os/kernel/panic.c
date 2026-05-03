// panic.c：实现最小内核 panic 输出与停机逻辑
#include "panic.h"
#include "vga.h"

// 最小 panic 实现：打印信息后关闭中断并停机
void kernel_panic(const char* message) {
    print_string("KERNEL PANIC: ");
    print_string(message);
    print_char('\n');

    // 关闭可屏蔽中断，避免 panic 后继续被外设打断
    __asm__ __volatile__("cli");

    // panic 后保持停机，表示系统已进入不可恢复状态
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
