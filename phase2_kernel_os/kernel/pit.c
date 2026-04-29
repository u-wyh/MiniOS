#include "io.h"
#include "pic.h"
#include "pit.h"
#include "vga.h"

// PIT 输入时钟频率
#define PIT_BASE_FREQUENCY 1193182
// PIT 模式/通道控制端口
#define PIT_COMMAND_PORT 0x43
// PIT 通道 0 数据端口
#define PIT_CHANNEL0_PORT 0x40

// 记录定时器 tick 次数，后续调度器会依赖它
static volatile unsigned int tick_count = 0;

// 配置 PIT，使其周期性产生 IRQ0 定时中断
void pit_init(unsigned int frequency) {
    unsigned int divisor;

    if (frequency == 0) {
        frequency = 20;
    }

    divisor = PIT_BASE_FREQUENCY / frequency;

    // 0x36 = 通道0 + 低字节/高字节 + 模式3 + 二进制计数
    outb(PIT_COMMAND_PORT, 0x36);
    outb(PIT_CHANNEL0_PORT, (unsigned char)(divisor & 0xFF));
    outb(PIT_CHANNEL0_PORT, (unsigned char)((divisor >> 8) & 0xFF));
}

// 定时器中断处理：当前阶段只维护 tick，避免频繁刷屏影响 Shell 观察
void timer_handler(void) {
    tick_count++;
    pic_send_eoi(0);
}
