#include "io.h"
#include "keyboard.h"
#include "pic.h"
#include "shell.h"
#include "vga.h"

// 最小输入缓冲区：用于把逐个按键积累成一整行字符串
static char input_buffer[128];
static char command_buffer[128];
// 记录当前已经输入到缓冲区的字符数
static int input_index = 0;

// 将最小 Set 1 扫描码映射成小写字母或数字
static char scancode_to_ascii(unsigned char scancode) {
    switch (scancode) {
        case 0x1E: return 'a';
        case 0x30: return 'b';
        case 0x2E: return 'c';
        case 0x20: return 'd';
        case 0x12: return 'e';
        case 0x21: return 'f';
        case 0x22: return 'g';
        case 0x23: return 'h';
        case 0x17: return 'i';
        case 0x24: return 'j';
        case 0x25: return 'k';
        case 0x26: return 'l';
        case 0x32: return 'm';
        case 0x31: return 'n';
        case 0x18: return 'o';
        case 0x19: return 'p';
        case 0x10: return 'q';
        case 0x13: return 'r';
        case 0x1F: return 's';
        case 0x14: return 't';
        case 0x16: return 'u';
        case 0x2F: return 'v';
        case 0x11: return 'w';
        case 0x2D: return 'x';
        case 0x15: return 'y';
        case 0x2C: return 'z';
        case 0x0B: return '0';
        case 0x02: return '1';
        case 0x03: return '2';
        case 0x04: return '3';
        case 0x05: return '4';
        case 0x06: return '5';
        case 0x07: return '6';
        case 0x08: return '7';
        case 0x09: return '8';
        case 0x0A: return '9';
        case 0x39: return ' ';
        default:   return '\0';
    }
}

// 键盘中断处理：读取 0x60 端口、维护输入缓冲、处理 Enter 提交
void keyboard_handler(void) {
    unsigned char scancode = inb(0x60);
    char ch;
    int i;

    // 释放码最高位为 1，本轮最小实现直接忽略
    if ((scancode & 0x80) != 0) {
        pic_send_eoi(1);
        return;
    }

    // Enter 键表示一行输入结束：补 '\0' 后交给 Shell 执行
    if (scancode == 0x1C) {
        input_buffer[input_index] = '\0';
        print_char('\n');

        for (i = 0; i <= input_index; i++) {
            command_buffer[i] = input_buffer[i];
        }
        input_index = 0;

        // 先结束本次键盘 IRQ，再执行命令，避免 run/exit 路径影响后续键盘中断。
        pic_send_eoi(1);
        shell_execute(command_buffer);
        return;
    }

    ch = scancode_to_ascii(scancode);
    if (ch != '\0') {
        // 将普通字符写入缓冲区，同时回显到屏幕
        input_buffer[input_index++] = ch;
        print_char(ch);

        // 保证缓冲区末尾始终留给 '\0'，避免后续字符串越界
        if (input_index >= 127) {
            input_index = 0;
        }
    }

    // 告知 PIC：IRQ1 已处理完成，否则后续键盘中断可能停止
    pic_send_eoi(1);
}
