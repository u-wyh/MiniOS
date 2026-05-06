// vga.c：封装 VGA 文本模式输出，供内核打印字符、字符串和清屏
#include "vga.h"

// VGA 文本模式显存基址
#define VGA_BUFFER ((volatile unsigned short*)0xB8000)
// VGA 屏幕宽度（列）
#define VGA_WIDTH 80
// VGA 屏幕高度（行）
#define VGA_HEIGHT 25
// 默认颜色：黑底亮白字
#define VGA_COLOR 0x0F

// 记录当前输出光标行
static int cursor_row = 0;
// 记录当前输出光标列
static int cursor_col = 0;

// 根据行列计算一维显存下标
static int vga_index(int row, int col) {
    return row * VGA_WIDTH + col;
}

// 清空整屏并将光标重置到左上角
void clear_screen(void) {
    for (int row = 0; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            VGA_BUFFER[vga_index(row, col)] = ((unsigned short)VGA_COLOR << 8) | (unsigned char)' ';
        }
    }

    cursor_row = 0;
    cursor_col = 0;
}

// 输出单字符并维护光标，支持换行 '\n'、回车 '\r' 与退格 '\b'
void print_char(char c) {
    if (c == '\n') {
        cursor_row++;
        cursor_col = 0;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\b') {
        print_backspace();
        return;
    } else {
        if (cursor_row < VGA_HEIGHT) {
            VGA_BUFFER[vga_index(cursor_row, cursor_col)] = ((unsigned short)VGA_COLOR << 8) | (unsigned char)c;
        }
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    // 最小实现：到底部后回到第一行，不做滚屏
    if (cursor_row >= VGA_HEIGHT) {
        cursor_row = 0;
    }
}

// 逐字符输出字符串，不依赖标准库
void print_string(const char* str) {
    int i = 0;
    while (str[i] != '\0') {
        print_char(str[i]);
        i++;
    }
}

// 删除当前光标前一个字符：仅处理当前最小单行命令输入场景
void print_backspace(void) {
    if (cursor_col == 0) {
        if (cursor_row == 0) {
            return;
        }

        cursor_row--;
        cursor_col = VGA_WIDTH - 1;
    } else {
        cursor_col--;
    }

    VGA_BUFFER[vga_index(cursor_row, cursor_col)] = ((unsigned short)VGA_COLOR << 8) | (unsigned char)' ';
}
