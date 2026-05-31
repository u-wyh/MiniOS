// vga.h：声明 VGA 文本模式输出接口
#ifndef VGA_H
#define VGA_H

// 清空 VGA 文本屏幕
void clear_screen(void);
// 在当前光标位置输出单个字符
void print_char(char c);
// 从当前光标位置连续输出字符串
void print_string(const char* str);
// 删除当前光标前一个字符，并把屏幕上的该位置清空
void print_backspace(void);
// 向上翻看历史屏幕内容；当前使用教学版固定步长翻页。
void vga_history_page_up(void);
// 向下翻看历史屏幕内容；翻到最新页后自动恢复实时跟随。
void vga_history_page_down(void);
// 主动回到最新输出位置；当前在用户重新输入时使用。
void vga_history_follow_tail(void);

#endif
