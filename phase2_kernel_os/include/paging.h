#ifndef PAGING_H
#define PAGING_H

// 页表标志位：存在、可写、用户可访问
#define PAGE_PRESENT 0x1
#define PAGE_WRITABLE 0x2
#define PAGE_USER 0x4

// 初始化最小分页结构并开启分页，当前阶段只建立内核 identity mapping
void paging_init(void);
// 把指定虚拟页映射到指定物理页，供用户空间等模块按需建立新映射
void map_page(unsigned int virtual_address, unsigned int physical_address, unsigned int flags);
// 解除一个虚拟页映射，供进程资源回收阶段释放地址空间占用
void unmap_page(unsigned int virtual_address);
// 返回分页是否已经开启，供 shell 做最小状态验证
int paging_is_enabled(void);
// 读取页目录物理地址，便于观察分页结构是否已建立
unsigned int paging_get_directory(void);
// 读取第一个页表物理地址
unsigned int paging_get_table0(void);
// 读取第二个页表物理地址
unsigned int paging_get_table1(void);
// 返回当前 identity mapping 覆盖的字节范围
unsigned int paging_get_identity_size(void);
// 返回内核高地址映射的虚拟基址，便于启动代码在开启分页后跳转到高地址别名
unsigned int paging_get_kernel_virtual_base(void);

#endif
