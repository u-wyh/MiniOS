#include "mm.h"
#include "vga.h"

// 物理内存按 4KB 固定页管理，这是 x86 内核里最常见的最小粒度
#define PAGE_SIZE 4096
// 当前只做最小实验系统，因此先管理 1024 个物理页
#define MAX_PAGES 1024
// 从 1MB 开始分配，避开传统实模式区域和早期内核加载区域
#define MEMORY_BASE 0x00100000

// 位图中 0 表示空闲页，1 表示已分配页
static unsigned char memory_bitmap[MAX_PAGES];

// 打印无符号整数，便于输出页统计信息
static void mm_print_uint(unsigned int value) {
    char digits[16];
    int index = 0;

    if (value == 0) {
        print_char('0');
        return;
    }

    while (value > 0) {
        digits[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (index > 0) {
        index--;
        print_char(digits[index]);
    }
}

// 初始化页位图：当前第一版简单把所有页都标记为空闲
void mm_init(void) {
    int i;

    for (i = 0; i < MAX_PAGES; i++) {
        memory_bitmap[i] = 0;
    }
}

// 顺序扫描位图，找到第一个空闲页并返回它的物理起始地址
void* alloc_page(void) {
    int i;

    for (i = 0; i < MAX_PAGES; i++) {
        if (memory_bitmap[i] == 0) {
            memory_bitmap[i] = 1;
            return (void*)(MEMORY_BASE + (i * PAGE_SIZE));
        }
    }

    return (void*)0;
}

// 根据页起始地址反推位图下标，并把该页重新标记为空闲
void free_page(void* addr) {
    unsigned int address = (unsigned int)addr;
    unsigned int index;

    if (address < MEMORY_BASE) {
        return;
    }

    index = (address - MEMORY_BASE) / PAGE_SIZE;
    if (index >= MAX_PAGES) {
        return;
    }

    memory_bitmap[index] = 0;
}

// 统计总页数、已用页数和空闲页数，供 shell 命令观察分配状态
void mem_stat(void) {
    unsigned int used_pages = 0;
    unsigned int free_pages = 0;
    int i;

    for (i = 0; i < MAX_PAGES; i++) {
        if (memory_bitmap[i] == 0) {
            free_pages++;
        } else {
            used_pages++;
        }
    }

    print_string("total pages: ");
    mm_print_uint(MAX_PAGES);
    print_char('\n');
    print_string("used pages: ");
    mm_print_uint(used_pages);
    print_char('\n');
    print_string("free pages: ");
    mm_print_uint(free_pages);
    print_char('\n');
}
