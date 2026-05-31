/* ===== RAMFS 槽位管理 ===== */

// 按路径查找 RAMFS 文件；找到返回槽位指针，否则返回空指针。
static struct ramfs_file* fs_ramfs_find(const char* path) {
    char normalized[MAX_FS_PATH_LEN];
    int i;
    int j;

    if (fs_normalize_path(path, normalized, MAX_FS_PATH_LEN) < 0) {
        return (struct ramfs_file*)0;
    }

    for (i = 0; i < MAX_RAMFS_FILES; i++) {
        struct ramfs_file* slot = &ramfs_files[i];

        if (slot->used == 0) {
            continue;
        }

        // RAMFS 槽位内部统一保存规范路径，这里直接按规范路径逐字节比对，避免走只读文件的兼容匹配分支。
        for (j = 0; j < MAX_FS_PATH_LEN; j++) {
            if (normalized[j] != slot->path[j]) {
                break;
            }

            if (normalized[j] == '\0') {
                return slot;
            }
        }
    }

    return (struct ramfs_file*)0;
}

// 统计当前已占用的 RAMFS 文件数量。
static uint32_t fs_ramfs_used_count(void) {
    uint32_t count = 0;
    int i;

    for (i = 0; i < MAX_RAMFS_FILES; i++) {
        if (ramfs_files[i].used != 0) {
            count++;
        }
    }

    return count;
}

// 清空一个 RAMFS 槽位，避免删除后残留路径/内容继续被观察到。
static void fs_ramfs_clear_slot(struct ramfs_file* file) {
    int i;

    if (file == (struct ramfs_file*)0) {
        return;
    }

    file->used = 0;
    file->size = 0;
    for (i = 0; i < MAX_FS_PATH_LEN; i++) {
        file->path[i] = '\0';
    }
    for (i = 0; i < MAX_RAMFS_FILE_SIZE + 1; i++) {
        file->content[i] = '\0';
    }
}

