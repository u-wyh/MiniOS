// idt.h：声明最小 IDT 初始化入口
#ifndef IDT_H
#define IDT_H

// 初始化最小 IDT 并注册软件中断入口
void idt_init(void);

#endif
