#ifndef PIT_H
#define PIT_H

// 初始化 PIT 通道 0 的周期性定时中断
void pit_init(unsigned int frequency);
// IRQ0 的 C 层处理函数，负责计时、输出与发送 EOI
void timer_handler(void);
// 读取当前 PIT tick 计数，供内核其他模块观察系统节拍
unsigned int pit_get_ticks(void);

#endif
