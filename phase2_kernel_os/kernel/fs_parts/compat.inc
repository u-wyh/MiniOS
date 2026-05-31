/* ===== 历史兼容/调试辅助接口 ===== */

// 列出当前所有文件，供 shell 的 ls 命令使用
void fs_list(void) {
    uint32_t i;

    for (i = 0; i < fs_count(); i++) {
        print_string(file_table[i].name);
        if (i + 1 < fs_count()) {
            print_char(' ');
        }
    }
    print_char('\n');
}

// 读取文件：当前最小实现直接复用 fs_find
struct file* fs_read(const char* name) {
    return fs_find(name);
}
