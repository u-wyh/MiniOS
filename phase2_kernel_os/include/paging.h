#ifndef PAGING_H
#define PAGING_H

// 初始化最小分页结构并开启分页，当前阶段只建立内核 identity mapping
void paging_init(void);
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

#endif
