/* ===== shell 基础数值/文件辅助 ===== */

// 输出十进制整数，便于显示 pid/tick/age。
static void shell_write_uint(int value) {
    char digits[16];
    int count = 0;
    unsigned int current;

    if (value == 0) {
        user_write("0");
        return;
    }

    if (value < 0) {
        user_write("-");
        current = (unsigned int)(-value);
    } else {
        current = (unsigned int)value;
    }

    while (current != 0) {
        digits[count++] = (char)('0' + (current % 10));
        current /= 10;
    }

    while (count > 0) {
        char one[2];
        count--;
        one[0] = digits[count];
        one[1] = '\0';
        user_write(one);
    }
}

// 把十进制字符串解析成整数；失败返回 0。
static int shell_atoi(const char* text, int* out_value) {
    int value = 0;
    int i = 0;

    if (text == (const char*)0 || text[0] == '\0' || out_value == (int*)0) {
        return 0;
    }

    while (text[i] != '\0') {
        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }
        value = value * 10 + (text[i] - '0');
        i++;
    }

    *out_value = value;
    return 1;
}

// 教学版文件名匹配：兼容 `/readme.txt`、`readme.txt` 以及不便输入标点时的 `readmetxt` 简写。
static int shell_file_name_matches(const char* input, const char* path) {
    int input_index = 0;
    int path_index = 0;

    if (input == (const char*)0 || path == (const char*)0) {
        return 0;
    }

    if (input[0] == '/') {
        input_index++;
    }
    if (path[0] == '/') {
        path_index++;
    }

    for (;;) {
        while (input[input_index] == '.') {
            input_index++;
        }
        while (path[path_index] == '.') {
            path_index++;
        }

        if (input[input_index] == '\0' || path[path_index] == '\0') {
            break;
        }

        if (input[input_index] != path[path_index]) {
            return 0;
        }

        input_index++;
        path_index++;
    }

    while (input[input_index] == '.') {
        input_index++;
    }
    while (path[path_index] == '.') {
        path_index++;
    }

    return input[input_index] == '\0' && path[path_index] == '\0';
}

// 返回当前教学版只读文本文件数量；文件清单仍只维护在共享宏里一份。
static int shell_builtin_file_count(void) {
    return (int)(sizeof(shell_builtin_files) / sizeof(shell_builtin_files[0]));
}

// 按索引读取共享只读文本文件，供 ls 遍历输出。
static const struct shell_builtin_text_file* shell_builtin_file_at(int index) {
    if (index < 0 || index >= (int)(sizeof(shell_builtin_files) / sizeof(shell_builtin_files[0]))) {
        return (const struct shell_builtin_text_file*)0;
    }

    return &shell_builtin_files[index];
}

// 按路径查找共享只读文本文件，供 cat 输出内容。
static const struct shell_builtin_text_file* shell_builtin_file_find(const char* path) {
    int i;

    if (path == (const char*)0 || path[0] == '\0') {
        return (const struct shell_builtin_text_file*)0;
    }

    for (i = 0; i < shell_builtin_file_count(); i++) {
        const struct shell_builtin_text_file* file = shell_builtin_file_at(i);

        if (file != (const struct shell_builtin_text_file*)0 && shell_file_name_matches(path, file->path)) {
            return file;
        }
    }

    return (const struct shell_builtin_text_file*)0;
}

