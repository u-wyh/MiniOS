// 最小字符串相等判断：仅供 syscall 层识别用户态 shell 输出的 "shell exit\n" 标记。
static int syscall_string_equals(const char* a, const char* b) {
    if (a == (const char*)0 || b == (const char*)0) {
        return 0;
    }

    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }

    return (*a == '\0' && *b == '\0') ? 1 : 0;
}

// 裸机环境下手动打印无符号整数，便于输出 pid/time
static void syscall_print_uint(unsigned int value) {
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

// 仅用于 Task31 调试：打印 waitpid/fork 相关返回值，便于观察父子执行路径
static void syscall_debug_result(const char* tag, unsigned int value) {
#if DEBUG_SYSCALL
    print_string("[syscall] ");
    print_string(tag);
    print_string("=");
    syscall_print_uint(value);
    print_char('\n');
#else
    (void)tag;
    (void)value;
#endif
}
