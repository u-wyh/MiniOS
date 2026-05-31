/* ===== 内置程序镜像 / 只读文本文件查询 ===== */

// 根据文件名查找文件，找到返回文件结构指针
struct file* fs_find(const char* name) {
    uint32_t i;

    if (name == (const char*)0 || name[0] == '\0') {
        return (struct file*)0;
    }

    for (i = 0; i < fs_count(); i++) {
        if (fs_str_equal(name, file_table[i].name) != 0) {
            return &file_table[i];
        }
    }

    return (struct file*)0;
}

// 按索引返回内置只读文本文件；越界时安全返回空指针。
const struct builtin_text_file* fs_builtin_file_at(uint32_t index) {
    if (index >= fs_builtin_text_file_count_only()) {
        return (const struct builtin_text_file*)0;
    }

    return &builtin_text_files[index];
}

// 按路径查找内置只读文本文件，供 shell 的 cat 和后续 open/read 雏形复用。
const struct builtin_text_file* fs_builtin_file_find(const char* path) {
    char normalized[MAX_FS_PATH_LEN];
    uint32_t i;
    uint32_t j;

    if (fs_normalize_path(path, normalized, MAX_FS_PATH_LEN) < 0) {
        return (const struct builtin_text_file*)0;
    }

    for (i = 0; i < fs_builtin_text_file_count_only(); i++) {
        for (j = 0; j < MAX_FS_PATH_LEN; j++) {
            char builtin_ch = builtin_text_files[i].path[j];

            if (normalized[j] != builtin_ch) {
                break;
            }

            if (builtin_ch == '\0') {
                return &builtin_text_files[i];
            }
        }
    }

    return (const struct builtin_text_file*)0;
}

