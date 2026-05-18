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

// 统一的内置只读文本文件清单：同时供内核文件表和用户态 shell 的 ls/cat 复用。
#define MINIOS_BUILTIN_TEXT_FILE_LIST(X)                                                                    \
    X("/readme.txt", "MiniOS Phase2 builtin read-only files\nThis is a teaching kernel, not a disk fs.\n") \
    X("/programs", "hello\necho\ncat\nloop\nloop_exit\nsleep_test\n")                                       \
    X("/help.txt", "help\nps\njobs\nuptime\nrun\nstart\nwait\nkill\nls\ncat\n")

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

#endif
