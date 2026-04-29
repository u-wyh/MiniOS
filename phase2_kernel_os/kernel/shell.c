#include "shell.h"
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

// Shell 初始化：当前阶段只需要打印首个提示符
void shell_init(void) {
    shell_print_prompt();
}

// 执行最小命令集合：help / clear / echo
void shell_execute(const char* line) {
    if (line[0] == '\0') {
        shell_print_prompt();
        return;
    }

    if (str_equal(line, "help")) {
        print_string("help  - show command list\n");
        print_string("clear - clear screen\n");
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

    print_string("Unknown command\n");
    shell_print_prompt();
}
