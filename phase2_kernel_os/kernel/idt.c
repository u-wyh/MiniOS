#include "idt.h"
#include "vga.h"

// IDT 总槽位数：x86 保护模式下为 256
#define IDT_SIZE 256
// 代码段选择子：与 GDT 中代码段保持一致（0x08）
#define KERNEL_CODE_SELECTOR 0x08
// 中断门属性：P=1, DPL=0, Type=1110(32位中断门)
#define IDT_TYPE_ATTR 0x8E

// 单个 IDT 描述符结构，必须紧凑布局避免编译器填充
struct idt_entry {
    unsigned short offset_low;
    unsigned short selector;
    unsigned char zero;
    unsigned char type_attr;
    unsigned short offset_high;
} __attribute__((packed));

// IDTR 结构：保存 IDT 的基地址和界限
struct idt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

// 全局 IDT 表
static struct idt_entry idt[IDT_SIZE];
// 记录 int 0x80 是否被成功处理，便于在 VGA 上可视化确认
static volatile int int80_handled = 0;

// 汇编 ISR 入口（int 0x80 对应）
extern void isr80(void);

// 设置指定中断向量的门描述符
static void idt_set_gate(unsigned char vector, unsigned int handler, unsigned short selector, unsigned char type_attr) {
    idt[vector].offset_low = (unsigned short)(handler & 0xFFFF);
    idt[vector].selector = selector;
    idt[vector].zero = 0;
    idt[vector].type_attr = type_attr;
    idt[vector].offset_high = (unsigned short)((handler >> 16) & 0xFFFF);
}

// 执行 lidt：把我们构建好的 IDT 地址加载到 CPU 的 IDTR
static void idt_load(void) {
    struct idt_ptr ptr;
    ptr.limit = (unsigned short)(sizeof(idt) - 1);
    ptr.base = (unsigned int)idt;
    __asm__ __volatile__("lidt %0" : : "m"(ptr));
}

// C 层中断处理函数：由汇编 ISR stub 调用
void interrupt_handler_80(void) {
    // 由中断路径输出提示，验证 IDT + ISR + iret 链路可用
    print_string("[OK] INT 0x80 TRIGGERED\n");
    int80_handled = 1;
}

// 初始化 IDT：清空、注册 int 0x80、加载 IDTR
void idt_init(void) {
    // 手动清空 IDT，不依赖标准库 memset
    for (int i = 0; i < IDT_SIZE; i++) {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].zero = 0;
        idt[i].type_attr = 0;
        idt[i].offset_high = 0;
    }

    // 注册 0x80 软件中断入口
    idt_set_gate(0x80, (unsigned int)isr80, KERNEL_CODE_SELECTOR, IDT_TYPE_ATTR);

    // 将 IDT 正式交给 CPU 使用
    idt_load();
}

int idt_was_int80_handled(void) {
    return int80_handled;
}
