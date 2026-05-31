/* ===== 对外暴露的统一文件可见性接口 ===== */

// 返回当前“可见文件”数量：内置只读文件和 RAMFS 文件都会出现在同一份列表里。
uint32_t fs_builtin_file_count(void) {
    return fs_builtin_text_file_count_only() + fs_ramfs_used_count();
}

// 按索引导出一个可见文件的路径和大小，供教学版用户态 ls syscall 复用。
int fs_builtin_file_info(int index, char* path_buf, int max_len) {
    uint32_t builtin_count;
    const struct builtin_text_file* builtin_file;
    int ramfs_index;
    int seen_ramfs = 0;
    int i;

    if (index < 0) {
        return -1;
    }

    builtin_count = fs_builtin_text_file_count_only();
    if ((uint32_t)index < builtin_count) {
        builtin_file = fs_builtin_file_at((uint32_t)index);
        if (builtin_file == (const struct builtin_text_file*)0) {
            return -2;
        }

        if (fs_copy_text_path(builtin_file->path, path_buf, max_len) < 0) {
            return -3;
        }

        return (int)builtin_file->size;
    }

    ramfs_index = index - (int)builtin_count;
    for (i = 0; i < MAX_RAMFS_FILES; i++) {
        if (ramfs_files[i].used == 0) {
            continue;
        }

        if (seen_ramfs == ramfs_index) {
            if (fs_copy_text_path(ramfs_files[i].path, path_buf, max_len) < 0) {
                return -3;
            }

            return (int)ramfs_files[i].size;
        }

        seen_ramfs++;
    }

    return -2;
}

// 按路径导出一个当前可见文件的最小元信息，供教学版 SYS_STAT 复用。
int fs_builtin_file_stat(const char* path, struct minios_stat* out_stat) {
    struct ramfs_file* ramfs_file;
    const struct builtin_text_file* file;

    if (out_stat == (struct minios_stat*)0) {
        return -1;
    }

    ramfs_file = fs_ramfs_find(path);
    if (ramfs_file != (struct ramfs_file*)0) {
        out_stat->size = ramfs_file->size;
        out_stat->type = MINIOS_FILE_TYPE_RAMFS_TEXT;
        return 0;
    }

    file = fs_builtin_file_find(path);
    if (file == (const struct builtin_text_file*)0) {
        return -2;
    }

    out_stat->size = file->size;
    out_stat->type = MINIOS_FILE_TYPE_READONLY_TEXT;
    return 0;
}

