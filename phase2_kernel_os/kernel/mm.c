#include "mm.h"
#include "vga.h"

// 物理内存按 4KB 固定页管理，这是 x86 内核里最常见的最小粒度
#define PAGE_SIZE 4096
// 当前只做最小实验系统，因此先管理 1024 个物理页
#define MAX_PAGES 1024
// kmalloc 当前把每个页切成固定 32B 小块，先验证小对象分配链路
#define KMALLOC_BLOCK_SIZE 32

// 最小空闲块结构：空闲块之间通过 next 串成单链表
struct block {
    struct block* next;
};

// 链接脚本导出的内核镜像结束地址，用于跳过已经被内核占用的物理页
extern unsigned char _kernel_end;

// 位图中 0 表示空闲页，1 表示已分配页
static unsigned char memory_bitmap[MAX_PAGES];
// kmalloc 的空闲块链表头指针
static struct block* free_list = (struct block*)0;
// 记录当前页分配器真正开始分配的物理基址，确保不会覆盖内核自身
static unsigned int memory_base = 0;

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

// 向上按 4KB 对齐地址，保证页分配器返回的始终是标准页边界
static unsigned int align_up_to_page(unsigned int address) {
    return (address + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

// 初始化页位图：当前第一版简单把所有页都标记为空闲
void mm_init(void) {
    int i;

    // 从内核镜像末尾的下一页开始分配，避免页分配器踩坏内核代码/数据
    memory_base = align_up_to_page((unsigned int)&_kernel_end);

    for (i = 0; i < MAX_PAGES; i++) {
        memory_bitmap[i] = 0;
    }

    free_list = (struct block*)0;
}

// 顺序扫描位图，找到第一个空闲页并返回它的物理起始地址
void* alloc_page(void) {
    int i;

    for (i = 0; i < MAX_PAGES; i++) {
        if (memory_bitmap[i] == 0) {
            memory_bitmap[i] = 1;
            return (void*)(memory_base + (i * PAGE_SIZE));
        }
    }

    return (void*)0;
}

// 根据页起始地址反推位图下标，并把该页重新标记为空闲
void free_page(void* addr) {
    unsigned int address = (unsigned int)addr;
    unsigned int index;

    if (address < memory_base) {
        return;
    }

    index = (address - memory_base) / PAGE_SIZE;
    if (index >= MAX_PAGES) {
        return;
    }

    memory_bitmap[index] = 0;
}

// 向 free list 补充一整页固定大小的小块，为后续 kmalloc 提供可复用节点
static int refill_kmalloc_blocks(void) {
    unsigned char* page = (unsigned char*)alloc_page();
    struct block* block;
    int block_count;
    int i;

    if (page == (unsigned char*)0) {
        return 0;
    }

    block_count = PAGE_SIZE / KMALLOC_BLOCK_SIZE;
    for (i = 0; i < block_count; i++) {
        block = (struct block*)(page + (i * KMALLOC_BLOCK_SIZE));
        block->next = free_list;
        free_list = block;
    }

    return 1;
}

// 分配内核小块：当前只支持不超过 32B 的请求，并从 free list 头部取一个块
void* kmalloc(unsigned int size) {
    struct block* block;

    if (size == 0 || size > KMALLOC_BLOCK_SIZE) {
        return (void*)0;
    }

    if (free_list == (struct block*)0) {
        if (refill_kmalloc_blocks() == 0) {
            return (void*)0;
        }
    }

    block = free_list;
    free_list = free_list->next;
    block->next = (struct block*)0;

    return (void*)block;
}

// 释放内核小块：把块重新挂回 free list 头部，供后续 kmalloc 复用
void kfree(void* ptr) {
    struct block* block = (struct block*)ptr;

    if (ptr == (void*)0) {
        return;
    }

    block->next = free_list;
    free_list = block;
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
