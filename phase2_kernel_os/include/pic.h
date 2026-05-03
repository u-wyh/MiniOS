// pic.h：声明 8259 PIC 的重映射、屏蔽和 EOI 接口
#ifndef PIC_H
#define PIC_H

// 重新映射 PIC 中断向量，并屏蔽当前阶段不需要的硬件中断
void pic_remap(void);
// 向 PIC 发送 EOI，告知当前 IRQ 已处理完成
void pic_send_eoi(unsigned char irq);

#endif
