// fs.h：声明内置程序镜像表与只读文本文件表的最小接口
#ifndef FS_H
#define FS_H

#include <stdint.h>

// 最小程序镜像文件结构：文件名、数据指针、字节大小
struct file {
    const char* name;
    void* data;
    uint32_t size;
};

// 教学版只读文本文件结构：保存路径、内容和字节大小，供 ls/cat 与后续 read 接口复用。
struct builtin_text_file {
    const char* path;
    const char* content;
    uint32_t size;
};

// 教学版文件类型：当前最小支持内置只读文本文件与 RAMFS 内存文本文件。
#define MINIOS_FILE_TYPE_READONLY_TEXT 1
// 教学版 RAMFS 文件类型：表示运行时创建的内存文本文件。
#define MINIOS_FILE_TYPE_RAMFS_TEXT 2

// 教学版路径长度上限：同时供 RAMFS 路径、fd 记录路径和用户态文件 syscall 复用。
#define MAX_FS_PATH_LEN 32
// 教学版 RAMFS 文件数量上限：当前使用固定数组，不做动态扩容。
#define MAX_RAMFS_FILES 8
// 教学版 RAMFS 单文件内容上限：需要容纳 readme/cat/ls/stat 的重定向结果，超过上限直接拒绝写入。
#define MAX_RAMFS_FILE_SIZE 256

// 教学版 RAMFS 文件槽位：记录是否占用、规范路径、文本内容与当前大小。
struct ramfs_file {
    int used;
    char path[MAX_FS_PATH_LEN];
    char content[MAX_RAMFS_FILE_SIZE + 1];
    uint32_t size;
};

// 教学版 stat 结构：当前只暴露最小文件大小与类型，不引入 inode/权限/时间戳。
struct minios_stat {
    uint32_t size;
    uint32_t type;
};

// 统一的内置只读文本文件清单：同时供内核文件表和用户态 shell 的 ls/cat 复用。
#define MINIOS_BUILTIN_TEXT_FILE_LIST(X)                                                                    \
    X("/readme.txt", "MiniOS Phase2 builtin read-only files\nThis is a teaching kernel, not a disk fs.\n") \
    X("/programs", "hello\necho\nls\ncat\nstat\nwritefile\nappend\nwc\ngrep\nhead\ntail\nsort\npipe_test\ndup2_test\nfork_fd_test\npipe_fork_dup2_test\npipe_close_test\nexec_fd_test\npipeline_demo\nexec_args_test\npipeline_args_demo\nmini_pipeline\npipe_multi_test\nloop\nloop_exit\nsleep_test\n")    \
    X("/help.txt", "help\nps\njobs\nuptime\nrun\nstart\nwait\nkill\nls\ncat\ntouch\nwritefile\nappend\nrm\n")

// 根据程序镜像文件名查找文件，找到返回文件指针，否则返回空指针
struct file* fs_find(const char* name);
// 列出当前程序镜像表中的所有文件名
void fs_list(void);
// 读取指定程序镜像，返回文件结构指针；失败返回空指针
struct file* fs_read(const char* name);

// 返回当前内置只读文本文件数量，供 shell 的 ls 遍历复用。
uint32_t fs_builtin_file_count(void);
// 按索引读取内置只读文本文件；越界时返回空指针。
const struct builtin_text_file* fs_builtin_file_at(uint32_t index);
// 按路径查找内置只读文本文件；找不到时返回空指针。
const struct builtin_text_file* fs_builtin_file_find(const char* path);
// 按索引把当前可见文件路径复制到缓冲区，并返回文件大小；内置只读文件和 RAMFS 文件都会参与枚举。
int fs_builtin_file_info(int index, char* path_buf, int max_len);
// 按路径查询一个当前可见文件的教学版元信息；成功返回 0，失败返回负值。
int fs_builtin_file_stat(const char* path, struct minios_stat* out_stat);
// 按路径与 offset 读取一个当前可见文本文件的内容；成功返回字节数，EOF 返回 0。
int fs_read_text_file(const char* path, uint32_t offset, char* out_buf, int max_len);
// 按路径与 offset 写入一个 RAMFS 文本文件；成功返回写入字节数，失败返回负值。
int fs_write_text_file(const char* path, uint32_t offset, const char* in_buf, int size);
// 创建一个空 RAMFS 文件；成功返回 0，失败返回负值。
int fs_create_ramfs_file(const char* path);
// 覆盖写入一个 RAMFS 文件；成功返回 0，失败返回负值。
int fs_write_ramfs_file(const char* path, const char* content);
// 追加写入一个 RAMFS 文件；成功返回追加字节数，失败返回负值。
int fs_append_ramfs_file(const char* path, const char* content);
// 删除一个 RAMFS 文件；成功返回 0，失败返回负值。
int fs_remove_ramfs_file(const char* path);

#endif
