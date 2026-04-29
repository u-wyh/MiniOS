#ifndef IDT_H
#define IDT_H

// 初始化最小 IDT 并注册软件中断入口
void idt_init(void);
// 查询 int 0x80 是否已进入并完成处理
int idt_was_int80_handled(void);

#endif
