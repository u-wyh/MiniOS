// keyboard.h：声明键盘中断处理和命令行输入相关接口
#ifndef KEYBOARD_H
#define KEYBOARD_H

// 键盘 IRQ1 的 C 层处理函数：读取扫描码、映射字符并发送 EOI
void keyboard_handler(void);

#endif
