/* ===== 对外暴露的文本读写接口 ===== */

// 按路径和 offset 读取一个当前可见文本文件的内容；当前统一服务内置只读文件与 RAMFS 文件。
int fs_read_text_file(const char* path, uint32_t offset, char* out_buf, int max_len) {
    struct ramfs_file* ramfs_file;
    const struct builtin_text_file* file;
    const char* content;
    uint32_t size;
    uint32_t remain;
    uint32_t to_copy;
    uint32_t i;

    if (out_buf == (char*)0) {
        return -1;
    }

    if (max_len < 0) {
        return -2;
    }

    if (max_len == 0) {
        return 0;
    }

    ramfs_file = fs_ramfs_find(path);
    if (ramfs_file != (struct ramfs_file*)0) {
        content = ramfs_file->content;
        size = ramfs_file->size;
    } else {
        file = fs_builtin_file_find(path);
        if (file == (const struct builtin_text_file*)0) {
            return -3;
        }
        content = file->content;
        size = file->size;
    }

    if (offset >= size) {
        return 0;
    }

    remain = size - offset;
    to_copy = (uint32_t)max_len;
    if (to_copy > remain) {
        to_copy = remain;
    }

    for (i = 0; i < to_copy; i++) {
        out_buf[i] = content[offset + i];
    }

    return (int)to_copy;
}

// 按路径与 offset 覆盖写入一个 RAMFS 文本文件：当前只允许写 RAMFS，内置只读文件一律拒绝。
int fs_write_text_file(const char* path, uint32_t offset, const char* in_buf, int size) {
    struct ramfs_file* file;
    uint32_t end_offset;
    int i;

    if (in_buf == (const char*)0) {
        return -1;
    }

    if (size < 0) {
        return -2;
    }

    if (size == 0) {
        return 0;
    }

    file = fs_ramfs_find(path);
    if (file == (struct ramfs_file*)0) {
        if (fs_builtin_file_find(path) != (const struct builtin_text_file*)0) {
            return -3;
        }
        return -4;
    }

    end_offset = offset + (uint32_t)size;
    if (end_offset > MAX_RAMFS_FILE_SIZE) {
        return -5;
    }

    for (i = 0; i < size; i++) {
        file->content[offset + (uint32_t)i] = in_buf[i];
    }

    file->size = end_offset;
    file->content[file->size] = '\0';
    for (i = (int)file->size + 1; i < MAX_RAMFS_FILE_SIZE + 1; i++) {
        file->content[i] = '\0';
    }

    return size;
}

// 创建一个空 RAMFS 文件：当前要求路径不存在，且不能与内置只读文件同名。
int fs_create_ramfs_file(const char* path) {
    char normalized[MAX_FS_PATH_LEN];
    int i;

    if (fs_normalize_path(path, normalized, MAX_FS_PATH_LEN) < 0) {
        return -1;
    }

    if (fs_builtin_file_find(normalized) != (const struct builtin_text_file*)0) {
        return -2;
    }

    if (fs_ramfs_find(normalized) != (struct ramfs_file*)0) {
        return -3;
    }

    for (i = 0; i < MAX_RAMFS_FILES; i++) {
        int j;

        if (ramfs_files[i].used != 0) {
            continue;
        }

        ramfs_files[i].used = 1;
        for (j = 0; j < MAX_FS_PATH_LEN; j++) {
            ramfs_files[i].path[j] = normalized[j];
            if (normalized[j] == '\0') {
                break;
            }
        }
        ramfs_files[i].path[MAX_FS_PATH_LEN - 1] = '\0';
        ramfs_files[i].size = 0;
        ramfs_files[i].content[0] = '\0';
        return 0;
    }

    return -4;
}

// 覆盖写入一个 RAMFS 文件：当前只支持纯文本，超过上限直接失败。
int fs_write_ramfs_file(const char* path, const char* content) {
    struct ramfs_file* file;
    int length;
    int i;

    if (content == (const char*)0) {
        return -1;
    }

    file = fs_ramfs_find(path);
    if (file == (struct ramfs_file*)0) {
        if (fs_builtin_file_find(path) != (const struct builtin_text_file*)0) {
            return -2;
        }
        return -3;
    }

    length = fs_copy_text_path(content, file->content, MAX_RAMFS_FILE_SIZE + 1);
    if (length < 0) {
        return -4;
    }

    file->size = (uint32_t)length;
    for (i = length; i < MAX_RAMFS_FILE_SIZE + 1; i++) {
        if (i == length) {
            file->content[i] = '\0';
            continue;
        }
        file->content[i] = '\0';
    }
    return 0;
}

// 追加写入一个 RAMFS 文件：保留旧内容并把新文本拼接到文件末尾；超过容量时失败且不修改原内容。
int fs_append_ramfs_file(const char* path, const char* content) {
    struct ramfs_file* file;
    int length;
    uint32_t old_size;
    uint32_t new_size;
    int i;

    if (content == (const char*)0) {
        return -1;
    }

    file = fs_ramfs_find(path);
    if (file == (struct ramfs_file*)0) {
        if (fs_builtin_file_find(path) != (const struct builtin_text_file*)0) {
            return -2;
        }
        return -3;
    }

    length = fs_text_length(content);
    if (length < 0) {
        return -4;
    }

    if (length == 0) {
        return 0;
    }

    old_size = file->size;
    new_size = old_size + (uint32_t)length;
    if (new_size > MAX_RAMFS_FILE_SIZE) {
        return -5;
    }

    for (i = 0; i < length; i++) {
        file->content[old_size + (uint32_t)i] = content[i];
    }
    file->size = new_size;
    file->content[file->size] = '\0';
    return length;
}

// 删除一个 RAMFS 文件：当前仅支持删除运行时创建的内存文件，不允许删除内置只读文件。
int fs_remove_ramfs_file(const char* path) {
    struct ramfs_file* file = fs_ramfs_find(path);

    if (file == (struct ramfs_file*)0) {
        if (fs_builtin_file_find(path) != (const struct builtin_text_file*)0) {
            return -2;
        }
        return -1;
    }

    fs_ramfs_clear_slot(file);
    return 0;
}

