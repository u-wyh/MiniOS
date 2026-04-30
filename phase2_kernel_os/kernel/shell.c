#include "panic.h"
#include "pit.h"
#include "shell.h"
#include "task.h"
#include "vga.h"

// 打印统一命令提示符，便于用户看到下一次输入位置
static void shell_print_prompt(void) {
    print_string("MiniOS> ");
}

// 比较两个字符串是否完全相等，不依赖 libc 的 strcmp
static int str_equal(const char* a, const char* b) {
    int i = 0;

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }

    return a[i] == '\0' && b[i] == '\0';
}

// 判断字符串是否以指定前缀开头，用于识别 echo 命令
static int str_starts_with(const char* s, const char* prefix) {
    int i = 0;

    while (prefix[i] != '\0') {
        if (s[i] != prefix[i]) {
            return 0;
        }
        i++;
    }

    return 1;
}

// 跳过指定前缀，返回命令参数起始位置
static const char* skip_prefix(const char* s, const char* prefix) {
    int i = 0;

    while (prefix[i] != '\0') {
        i++;
    }

    return s + i;
}

// 打印无符号整数，用于在裸机环境下输出 tick 等数值
static void print_uint(unsigned int value) {
    char digits[16];
    int index = 0;

    if (value == 0) {
        print_char('0');
        return;
    }

    while (value > 0) {
        digits[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (index > 0) {
        index--;
        print_char(digits[index]);
    }
}

// Shell 初始化：当前阶段只需要打印首个提示符
void shell_init(void) {
    shell_print_prompt();
}

// 执行最小命令集合：help / clear / echo / task / about / tick / panic
void shell_execute(const char* line) {
    if (line[0] == '\0') {
        shell_print_prompt();
        return;
    }

    if (str_equal(line, "help")) {
        print_string("help  - show command list\n");
        print_string("clear - clear screen\n");
        print_string("test  - reserved command slot\n");
        print_string("task  - switch demo task\n");
        print_string("about - show kernel info\n");
        print_string("tick  - show pit ticks\n");
        print_string("panic - trigger kernel panic\n");
        print_string("echo  - print text\n");
        shell_print_prompt();
        return;
    }

    if (str_equal(line, "clear")) {
        clear_screen();
        shell_print_prompt();
        return;
    }

    if (str_starts_with(line, "echo ")) {
        print_string(skip_prefix(line, "echo "));
        print_char('\n');
        shell_print_prompt();
        return;
    }

    if (str_equal(line, "task")) {
        switch_task();
        print_char('\n');
        shell_print_prompt();
        return;
    }

    if (str_equal(line, "about")) {
        print_string("MiniOS Phase2 Kernel\n");
        print_string("Version: 0.1\n");
        print_string("Mode: Protected Mode\n");
        print_string("Features: VGA, GDT, IDT, PIT, Keyboard, Kernel Monitor\n");
        shell_print_prompt();
        return;
    }

    if (str_equal(line, "tick")) {
        print_string("tick: ");
        print_uint(pit_get_ticks());
        print_char('\n');
        shell_print_prompt();
        return;
    }

    if (str_equal(line, "panic")) {
        kernel_panic("manual panic triggered");
        return;
    }

    print_string("Unknown command\n");
    shell_print_prompt();
}
