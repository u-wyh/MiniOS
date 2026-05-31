/* ===== 文件系统内部辅助函数 ===== */

// 计算一个以 0 结尾文本的长度；当前只服务于内核文件系统里的最小文本处理。
static int fs_text_length(const char* text) {
    int length = 0;

    if (text == (const char*)0) {
        return -1;
    }

    while (text[length] != '\0') {
        length++;
    }

    return length;
}

// 统计文件数量，避免硬编码
static uint32_t fs_count(void) {
    return (uint32_t)(sizeof(file_table) / sizeof(file_table[0]));
}

// 统计内置只读文本文件数量，供可见文件总数与索引映射复用。
static uint32_t fs_builtin_text_file_count_only(void) {
    return (uint32_t)(sizeof(builtin_text_files) / sizeof(builtin_text_files[0]));
}

// 最小字符串比较，仅用于文件名匹配
static int fs_str_equal(const char* a, const char* b) {
    uint32_t i = 0;

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }

    return a[i] == '\0' && b[i] == '\0';
}

// 把内置文件路径安全复制到调用方缓冲区；缓冲区太小时返回错误。
static int fs_copy_text_path(const char* source, char* target, int max_len) {
    int i;

    if (source == (const char*)0 || target == (char*)0) {
        return -1;
    }

    if (max_len <= 0) {
        return -2;
    }

    for (i = 0; i < max_len - 1; i++) {
        target[i] = source[i];
        if (source[i] == '\0') {
            return i;
        }
    }

    target[max_len - 1] = '\0';
    if (source[max_len - 1] != '\0') {
        return -3;
    }

    return max_len - 1;
}

// 把输入路径标准化成以 '/' 开头的教学版规范路径，供 RAMFS 创建/删除/写入统一使用。
static int fs_normalize_path(const char* source, char* target, int max_len) {
    int source_index = 0;
    int target_index = 0;

    if (source == (const char*)0 || target == (char*)0) {
        return -1;
    }

    if (source[0] == '\0' || max_len <= 1) {
        return -2;
    }

    target[target_index++] = '/';
    if (source[0] == '/') {
        source_index = 1;
    }

    while (source[source_index] != '\0') {
        if (target_index >= max_len - 1) {
            target[max_len - 1] = '\0';
            return -3;
        }

        target[target_index++] = source[source_index++];
    }

    target[target_index] = '\0';
    if (target_index <= 1) {
        return -4;
    }

    return 0;
}

// 教学版文本文件路径匹配：兼容 `/readme.txt`、`readme.txt` 和 `readmetxt`。
static int fs_text_path_equal(const char* input, const char* path) {
    uint32_t input_index = 0;
    uint32_t path_index = 0;

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

