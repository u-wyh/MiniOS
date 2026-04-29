#include "io.h"
#include "pic.h"

// 主片 PIC 命令端口
#define PIC1_COMMAND 0x20
// 主片 PIC 数据端口
#define PIC1_DATA 0x21
// 从片 PIC 命令端口
#define PIC2_COMMAND 0xA0
// 从片 PIC 数据端口
#define PIC2_DATA 0xA1
// 初始化控制字 1：开始初始化流程并要求后续发送 ICW4
#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
// 初始化控制字 4：使用 8086/88 模式
#define ICW4_8086 0x01
// End Of Interrupt 命令值
#define PIC_EOI 0x20

// 重映射 PIC，避免 IRQ0 默认落在 0x08 与 CPU 异常冲突
void pic_remap(void) {
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    // 主片从 0x20 开始，从片从 0x28 开始
    outb(PIC1_DATA, 0x20);
    io_wait();
    outb(PIC2_DATA, 0x28);
    io_wait();

    // 告诉主片 IRQ2 连接了从片，并告诉从片其级联身份
    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    // 当前阶段放开 IRQ0 和 IRQ1，其余硬件中断保持屏蔽
    outb(PIC1_DATA, 0xFC);
    outb(PIC2_DATA, 0xFF);
}

// 告知 PIC 当前 IRQ 已完成，否则后续中断不会继续到达
void pic_send_eoi(unsigned char irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}
