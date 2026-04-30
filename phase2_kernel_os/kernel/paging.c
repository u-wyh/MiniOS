#include "mm.h"
#include "paging.h"

// x86 32 位分页中，一个页目录/页表都包含 1024 个表项
#define PAGE_ENTRY_COUNT 1024
// PDE/PTE 的 Present + Read/Write 标志
#define PAGE_FLAGS_PRESENT_RW 0x3
// 当前最小实现先做前 8MB 的 identity mapping，覆盖内核和早期分配区域
#define IDENTITY_MAP_SIZE 0x00800000

// 记录分页结构物理地址，便于后续调试和扩展
static unsigned int* page_directory = (unsigned int*)0;
static unsigned int* page_table0 = (unsigned int*)0;
static unsigned int* page_table1 = (unsigned int*)0;
static int paging_enabled = 0;

// 手动清空一个页大小的分页结构，不依赖标准库 memset
static void paging_zero_page(unsigned int* page) {
    int i;

    for (i = 0; i < PAGE_ENTRY_COUNT; i++) {
        page[i] = 0;
    }
}

// 填充单个页表，让虚拟地址和物理地址保持一一对应
static void paging_fill_identity_table(unsigned int* page_table, unsigned int base_address) {
    int i;

    for (i = 0; i < PAGE_ENTRY_COUNT; i++) {
        page_table[i] = (base_address + (i * 0x1000)) | PAGE_FLAGS_PRESENT_RW;
    }
}

// 初始化页目录和页表，并通过 CR3/CR0 正式开启分页
void paging_init(void) {
    unsigned int cr0_value;

    page_directory = (unsigned int*)alloc_page();
    page_table0 = (unsigned int*)alloc_page();
    page_table1 = (unsigned int*)alloc_page();

    if (page_directory == (unsigned int*)0 || page_table0 == (unsigned int*)0 || page_table1 == (unsigned int*)0) {
        return;
    }

    paging_zero_page(page_directory);
    paging_zero_page(page_table0);
    paging_zero_page(page_table1);

    // 第一个页表映射 0~4MB，确保当前内核代码、数据和 VGA 都能继续访问
    paging_fill_identity_table(page_table0, 0x00000000);
    // 第二个页表映射 4MB~8MB，覆盖当前早期页分配器可返回的物理页范围
    paging_fill_identity_table(page_table1, 0x00400000);

    page_directory[0] = ((unsigned int)page_table0) | PAGE_FLAGS_PRESENT_RW;
    page_directory[1] = ((unsigned int)page_table1) | PAGE_FLAGS_PRESENT_RW;

    // CR3 指向页目录物理基址；当前是 identity mapping，因此地址可直接使用
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(page_directory));

    // 打开 CR0.PG 位，CPU 之后会按照页目录/页表做地址翻译
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0_value));
    cr0_value |= 0x80000000;
    __asm__ __volatile__("mov %0, %%cr0" : : "r"(cr0_value));
    paging_enabled = 1;
}

// 返回分页是否已经打开，便于用命令行确认 PG 位初始化结果
int paging_is_enabled(void) {
    return paging_enabled;
}

// 返回页目录物理地址，用于观察 CR3 将指向哪一页
unsigned int paging_get_directory(void) {
    return (unsigned int)page_directory;
}

// 返回第一个页表物理地址，对应 0~4MB 映射
unsigned int paging_get_table0(void) {
    return (unsigned int)page_table0;
}

// 返回第二个页表物理地址，对应 4MB~8MB 映射
unsigned int paging_get_table1(void) {
    return (unsigned int)page_table1;
}

// 返回当前 identity mapping 覆盖大小，便于 shell 打印映射范围
unsigned int paging_get_identity_size(void) {
    return IDENTITY_MAP_SIZE;
}
