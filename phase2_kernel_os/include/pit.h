// pit.h：声明 PIT 定时器初始化、tick 查询和 IRQ0 处理接口
#ifndef PIT_H
#define PIT_H

// 教学版 PIT 默认频率：每秒 20 个 tick，约每 50ms 触发一次 IRQ0。
#define PIT_DEFAULT_FREQUENCY 20

// 初始化 PIT 通道 0 的周期性定时中断
void pit_init(unsigned int frequency);
// IRQ0 的 C 层处理函数：负责计时、调度与发送 EOI，并返回恢复目标 ESP
unsigned int timer_handler(unsigned int current_esp);
// 读取当前 PIT tick 计数，供内核其他模块观察系统节拍
unsigned int pit_get_ticks(void);
// 读取当前 PIT 配置频率，供 uptime 等时间展示做最小换算。
unsigned int pit_get_frequency(void);

#endif
