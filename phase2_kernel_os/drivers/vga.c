// vga.c：封装 VGA 文本模式输出，供内核打印字符、字符串和清屏
#include "vga.h"

// VGA 文本模式显存基址
#define VGA_BUFFER ((volatile unsigned short*)0xB8000)
// VGA 屏幕宽度（列）
#define VGA_WIDTH 80
// VGA 屏幕高度（行）
#define VGA_HEIGHT 25
// 教学版历史缓冲保留行数：足够查看 help/ps 等长输出，不引入复杂动态分配。
#define VGA_HISTORY_LINES 200
// 默认颜色：黑底亮白字
#define VGA_COLOR 0x0F
// 历史翻页步长：每次按 PageUp/PageDown 滚动 10 行，兼顾查看效率与定位精度。
#define VGA_HISTORY_PAGE_STEP 10

// 教学版历史屏幕缓冲：保留最近 200 行字符/颜色对。
static unsigned short vga_history[VGA_HISTORY_LINES][VGA_WIDTH];

// 记录当前输出光标所在的历史行。
static int cursor_row = 0;
// 记录当前输出光标列
static int cursor_col = 0;
// 记录历史缓冲当前已经使用到的总行数。
static int history_rows_used = 1;
// 记录当前屏幕窗口显示的历史起始行。
static int viewport_top_row = 0;
// 记录当前是否始终跟随最新输出；翻历史时会临时关闭。
static int viewport_follow_tail = 1;

// 根据行列计算一维显存下标
static int vga_index(int row, int col) {
    return row * VGA_WIDTH + col;
}

// 把指定历史行清空为空格，供 clear/new line/滚动回收统一复用。
static void vga_clear_history_row(int row) {
    int col;

    for (col = 0; col < VGA_WIDTH; col++) {
        vga_history[row][col] = ((unsigned short)VGA_COLOR << 8) | (unsigned char)' ';
    }
}

// 把当前历史窗口重新绘制到真实 VGA 显存。
static void vga_render_viewport(void) {
    int row;
    int col;

    for (row = 0; row < VGA_HEIGHT; row++) {
        int history_row = viewport_top_row + row;

        for (col = 0; col < VGA_WIDTH; col++) {
            if (history_row >= 0 && history_row < history_rows_used) {
                VGA_BUFFER[vga_index(row, col)] = vga_history[history_row][col];
            } else {
                VGA_BUFFER[vga_index(row, col)] = ((unsigned short)VGA_COLOR << 8) | (unsigned char)' ';
            }
        }
    }
}

// 根据当前光标位置，把视口自动对齐到最新输出末尾。
static void vga_sync_viewport_to_tail(void) {
    int max_top = 0;

    if (history_rows_used > VGA_HEIGHT) {
        max_top = history_rows_used - VGA_HEIGHT;
    }

    if (cursor_row >= VGA_HEIGHT - 1) {
        viewport_top_row = cursor_row - (VGA_HEIGHT - 1);
    } else {
        viewport_top_row = 0;
    }

    if (viewport_top_row > max_top) {
        viewport_top_row = max_top;
    }

    if (viewport_top_row < 0) {
        viewport_top_row = 0;
    }
}

// 当历史缓冲写满时，把 200 行整体上移一行，保留最新内容窗口。
static void vga_history_drop_oldest_line(void) {
    int row;
    int col;

    for (row = 1; row < VGA_HISTORY_LINES; row++) {
        for (col = 0; col < VGA_WIDTH; col++) {
            vga_history[row - 1][col] = vga_history[row][col];
        }
    }

    vga_clear_history_row(VGA_HISTORY_LINES - 1);
    cursor_row = VGA_HISTORY_LINES - 1;
    if (history_rows_used > 0) {
        history_rows_used = VGA_HISTORY_LINES;
    }
    if (viewport_top_row > 0) {
        viewport_top_row--;
    }
}

// 确保即将写入的历史行已经存在；必要时扩展历史使用范围或淘汰最旧一行。
static void vga_ensure_cursor_row_visible(void) {
    while (cursor_row >= VGA_HISTORY_LINES) {
        vga_history_drop_oldest_line();
    }

    while (history_rows_used <= cursor_row && history_rows_used < VGA_HISTORY_LINES) {
        vga_clear_history_row(history_rows_used);
        history_rows_used++;
    }
}

// 清空整屏并将光标重置到左上角
void clear_screen(void) {
    int row;

    for (row = 0; row < VGA_HISTORY_LINES; row++) {
        vga_clear_history_row(row);
    }

    cursor_row = 0;
    cursor_col = 0;
    history_rows_used = 1;
    viewport_top_row = 0;
    viewport_follow_tail = 1;
    vga_render_viewport();
}

// 在继续正常输出前，必要时把视口拉回到最新位置，保证新内容始终可见。
static void vga_refresh_after_output(void) {
    if (viewport_follow_tail != 0) {
        vga_sync_viewport_to_tail();
    }
    vga_render_viewport();
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
        vga_ensure_cursor_row_visible();
        vga_history[cursor_row][cursor_col] = ((unsigned short)VGA_COLOR << 8) | (unsigned char)c;
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    vga_ensure_cursor_row_visible();
    vga_refresh_after_output();
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

    vga_ensure_cursor_row_visible();
    vga_history[cursor_row][cursor_col] = ((unsigned short)VGA_COLOR << 8) | (unsigned char)' ';
    vga_refresh_after_output();
}

// 向上翻看历史内容：每次固定回退若干行，并停止自动跟随最新输出。
void vga_history_page_up(void) {
    if (viewport_top_row <= 0) {
        return;
    }

    viewport_follow_tail = 0;
    viewport_top_row -= VGA_HISTORY_PAGE_STEP;
    if (viewport_top_row < 0) {
        viewport_top_row = 0;
    }
    vga_render_viewport();
}

// 向下翻看历史内容；一旦回到最新页，就恢复自动跟随。
void vga_history_page_down(void) {
    int max_top = 0;

    if (history_rows_used > VGA_HEIGHT) {
        max_top = history_rows_used - VGA_HEIGHT;
    }

    if (viewport_top_row >= max_top) {
        viewport_follow_tail = 1;
        vga_sync_viewport_to_tail();
        vga_render_viewport();
        return;
    }

    viewport_top_row += VGA_HISTORY_PAGE_STEP;
    if (viewport_top_row >= max_top) {
        viewport_top_row = max_top;
        viewport_follow_tail = 1;
    }

    vga_render_viewport();
}

// 主动回到最新输出位置，供用户重新输入时取消历史查看模式。
void vga_history_follow_tail(void) {
    viewport_follow_tail = 1;
    vga_sync_viewport_to_tail();
    vga_render_viewport();
}
