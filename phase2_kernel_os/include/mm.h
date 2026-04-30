#ifndef MM_H
#define MM_H

// 初始化最小物理页分配器，建立页位图初始状态
void mm_init(void);
// 分配一个 4KB 物理页，成功返回页起始地址，失败返回空指针
void* alloc_page(void);
// 释放一个之前分配的物理页地址
void free_page(void* addr);
// 分配一个内核小块内存，当前最小实现基于固定大小块切分页
void* kmalloc(unsigned int size);
// 释放一个之前通过 kmalloc 获取的小块内存
void kfree(void* ptr);
// 输出当前页分配统计信息
void mem_stat(void);

#endif
