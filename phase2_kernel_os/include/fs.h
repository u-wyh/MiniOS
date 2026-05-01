#ifndef FS_H
#define FS_H

#include <stdint.h>

// 最小内存文件结构：文件名、数据指针、字节大小
struct file {
    const char* name;
    void* data;
    uint32_t size;
};

// 根据文件名查找文件，找到返回文件指针，否则返回空指针
struct file* fs_find(const char* name);
// 列出当前 ramfs 中的所有文件名
void fs_list(void);
// 读取指定文件，返回文件结构指针；失败返回空指针
struct file* fs_read(const char* name);

#endif
