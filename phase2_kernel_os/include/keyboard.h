// keyboard.h：声明键盘中断处理和命令行输入相关接口
#ifndef KEYBOARD_H
#define KEYBOARD_H

// 键盘 IRQ1 的 C 层处理函数：读取扫描码、映射字符并发送 EOI
void keyboard_handler(void);
// 从最小键盘输入缓冲区读取一个字符；无输入时返回 0
char keyboard_read_char(void);
// 阻塞等待一个字符；当前无输入时先让 CPU 休眠，直到后续中断把它唤醒
char keyboard_read_char_blocking(void);

#endif
