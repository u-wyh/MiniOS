// keyboard.c：处理中断驱动的键盘输入，并把整行命令交给 shell
#include "io.h"
#include "keyboard.h"
#include "pic.h"
#include "process.h"
#include "shell.h"
#include "vga.h"

// 最小输入缓冲区：用于把逐个按键积累成一整行字符串
static char input_buffer[128];
static char command_buffer[128];
// 记录当前已经输入到缓冲区的字符数
static int input_index = 0;

#define KBD_BUF_SIZE 128

// 最小键盘环形缓冲区：IRQ 产出字符，sys_read_char 从这里消费字符
static char kbd_buf[KBD_BUF_SIZE];
static unsigned int kbd_head = 0;
static unsigned int kbd_tail = 0;
// 记录扫描码当前是否处于“按下未释放”状态，用于忽略 typematic 或异常重复 make 码。
static unsigned char key_down[128];
// 记录左右 Shift 当前是否按下；供最小教学版标点输入复用，例如 Shift + '.' -> '>'。
static int shift_down = 0;
// 记录上一字节是否为 0xE0 扩展前缀，用于识别 PageUp/PageDown 等双字节扫描码。
static int extended_prefix = 0;

// 向最小键盘缓冲区写入一个字符；缓冲区满时丢弃新字符，保持已有输入不被覆盖
static void keyboard_buffer_put(char ch) {
    unsigned int next_head = (kbd_head + 1U) % KBD_BUF_SIZE;

    if (next_head == kbd_tail) {
        return;
    }

    kbd_buf[kbd_head] = ch;
    kbd_head = next_head;
}

// 从最小键盘缓冲区读取一个字符；当前为空时返回 0 表示暂无输入
char keyboard_read_char(void) {
    char ch;

    if (kbd_head == kbd_tail) {
        return 0;
    }

    ch = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1U) % KBD_BUF_SIZE;
    return ch;
}

// 在最小教学版里用“sti + hlt”阻塞等待字符，避免用户态 shell 反复 syscall 忙等打满 CPU。
// 当前没有输入时，CPU 会先休眠；任意中断到来后再醒来重新检查缓冲区，直到拿到字符再返回。
char keyboard_read_char_blocking(void) {
    char ch;

    for (;;) {
        ch = keyboard_read_char();
        if (ch != 0) {
            return ch;
        }

        __asm__ __volatile__("sti; hlt; cli");
    }
}

// 将最小 Set 1 扫描码映射成教学版 shell 需要的小写字母、数字和少量标点。
// 当前补齐路径和重定向常用字符，例如 ','、'.'、'/' 与 Shift 组合得到的 '<'、'>'、'|'。
static char scancode_to_ascii(unsigned char scancode, int shift_active) {
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
        // 补齐最常用的减号/下划线输入，便于用户态文本工具接受 -n 这类最小参数。
        case 0x0C:
            if (shift_active != 0) {
                return '_';
            }
            return '-';
        case 0x33:
            if (shift_active != 0) {
                return '<';
            }
            return ',';
        case 0x34:
            if (shift_active != 0) {
                return '>';
            }
            return '.';
        case 0x35: return '/';
        case 0x2B:
            if (shift_active != 0) {
                return '|';
            }
            return '\\';
        default:   return '\0';
    }
}

// 键盘中断处理：读取 0x60 端口、维护输入缓冲、处理 Enter 提交
void keyboard_handler(void) {
    unsigned char scancode = inb(0x60);
    char ch;
    int i;
    // 用户 shell 等输入时可能处于 BLOCKED，并不一定是 current_process。
    // 只要存在正在等待 read_char 的用户进程，就不能把按键再交给内核 shell 回显/缓存。
    int user_input_active = (process_has_user_process() != 0 || process_current_pid() != 0 || process_has_read_char_waiter() != 0);

    // 0xE0 表示后续是扩展扫描码；当前只需要它来识别 PageUp/PageDown 这类历史翻页键。
    if (scancode == 0xE0) {
        extended_prefix = 1;
        pic_send_eoi(1);
        return;
    }

    // 处理扩展键：教学版当前只消费 PageUp/PageDown；其它扩展键直接忽略。
    if (extended_prefix != 0) {
        extended_prefix = 0;

        if (scancode == 0x49) {
            vga_history_page_up();
            pic_send_eoi(1);
            return;
        }

        if (scancode == 0x51) {
            vga_history_page_down();
            pic_send_eoi(1);
            return;
        }

        if ((scancode & 0x80) != 0) {
            pic_send_eoi(1);
            return;
        }

        pic_send_eoi(1);
        return;
    }

    // 释放码最高位为 1：把对应按键标记为“已释放”，避免下次正常按下被当成重复 make 码忽略。
    if ((scancode & 0x80) != 0) {
        unsigned char make_code = (unsigned char)(scancode & 0x7F);

        if (make_code < 128) {
            key_down[make_code] = 0;
        }
        if (make_code == 0x2A || make_code == 0x36) {
            shift_down = 0;
        }
        pic_send_eoi(1);
        return;
    }

    // 同一个键还没收到释放码时再次收到 make 码，视为重复触发并直接忽略。
    // 这样可避免用户态 shell 偶发出现 eexit / exxit 这类重复字符显示。
    if (scancode < 128 && key_down[scancode] != 0) {
        pic_send_eoi(1);
        return;
    }

    if (scancode < 128) {
        key_down[scancode] = 1;
    }

    if (scancode == 0x2A || scancode == 0x36) {
        shift_down = 1;
        pic_send_eoi(1);
        return;
    }

    // Enter 键表示一行输入结束：补 '\0' 后交给 Shell 执行
    if (scancode == 0x1C) {
        // 用户翻历史后重新输入命令时，自动回到最新输出区域，避免输入内容仍停留在旧视图外。
        vga_history_follow_tail();

        if (user_input_active != 0) {
            if (process_wake_read_char_waiter('\n') == 0) {
                keyboard_buffer_put('\n');
            }
            // 用户态 shell 会自己处理并回显换行，这里只入缓冲，避免重复回显和显示错位。
            pic_send_eoi(1);
            return;
        }

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

    // Backspace 退格键：同步删除输入缓冲中的最后一个字符，并清除屏幕回显
    if (scancode == 0x0E) {
        // 重新编辑命令时应退出历史查看模式，恢复到最新屏幕位置。
        vga_history_follow_tail();

        if (user_input_active != 0) {
            // 用户态下把退格键作为字符事件交给 shell 处理，保证“屏幕效果”和“命令缓冲”一致。
            if (process_wake_read_char_waiter('\b') == 0) {
                keyboard_buffer_put('\b');
            }
            pic_send_eoi(1);
            return;
        }

        if (input_index > 0) {
            input_index--;
            input_buffer[input_index] = '\0';
            print_backspace();
        }

        pic_send_eoi(1);
        return;
    }

    ch = scancode_to_ascii(scancode, shift_down);
    if (ch != '\0') {
        // 任意新的可打印输入都说明用户要继续交互，此时应从历史翻页视图回到最新输出位置。
        vga_history_follow_tail();

        // 先把可打印字符放入最小输入缓冲区，后续用户态 read_char syscall 从这里消费。
        // 当前阶段 IRQ 与 syscall 共享该缓冲区，暂未加锁，后续可用关中断或锁进一步完善。
        if (process_wake_read_char_waiter(ch) == 0) {
            keyboard_buffer_put(ch);
        }
        // 用户态 shell 会自己回显字符，这里只在内核 shell 前台时回显，避免双回显。
        if (user_input_active == 0) {
            print_char(ch);
        }

        // 只有内核 shell 在前台时，才继续把字符写入命令行缓冲区，避免污染用户态 shell 的输入。
        if (user_input_active == 0) {
            input_buffer[input_index++] = ch;

            // 保证缓冲区末尾始终留给 '\0'，避免后续字符串越界
            if (input_index >= 127) {
                input_index = 0;
            }
        }
    }

    // 告知 PIC：IRQ1 已处理完成，否则后续键盘中断可能停止
    pic_send_eoi(1);
}
