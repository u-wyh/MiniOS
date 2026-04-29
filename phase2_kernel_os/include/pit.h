#ifndef PIT_H
#define PIT_H

// 初始化 PIT 通道 0 的周期性定时中断
void pit_init(unsigned int frequency);
// IRQ0 的 C 层处理函数，负责计时、输出与发送 EOI
void timer_handler(void);

#endif
