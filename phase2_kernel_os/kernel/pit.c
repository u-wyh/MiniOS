// pit.c：实现 PIT 周期中断初始化与最小 tick/调度钩子
#include "io.h"
#include "pic.h"
#include "pit.h"
#include "sched.h"

// PIT 输入时钟频率
#define PIT_BASE_FREQUENCY 1193182
// PIT 模式/通道控制端口
#define PIT_COMMAND_PORT 0x43
// PIT 通道 0 数据端口
#define PIT_CHANNEL0_PORT 0x40
// 每 10 次 tick 触发一次任务切换，避免切换频率过高
#define PIT_SCHEDULE_INTERVAL 10

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

// 定时器中断处理：维护 tick，并按固定时间片触发最小任务调度
// 参数 current_esp 指向“旧任务完整中断现场”的栈顶，返回值则是应恢复的新任务 esp
unsigned int timer_handler(unsigned int current_esp) {
    tick_count++;

    // 只有调度器真正启用后，才在固定时间片边界切换任务；纯 shell 模式只累计 tick
    if (scheduler_is_enabled() != 0 && (tick_count % PIT_SCHEDULE_INTERVAL) == 0) {
        current_esp = schedule(current_esp);
    }

    pic_send_eoi(0);

    return current_esp;
}

// 返回当前累计 tick 数，供控制台命令读取真实系统节拍
unsigned int pit_get_ticks(void) {
    return tick_count;
}
