section .text
global isr80
extern interrupt_handler_80

isr80:
    ; 进入 ISR 后先保存通用寄存器，避免破坏被中断上下文
    pusha

    ; 调用 C 层中断处理函数，执行中断逻辑
    call interrupt_handler_80

    ; 恢复通用寄存器，确保返回后上下文一致
    popa

    ; 使用 iret 从中断返回，恢复 EIP/CS/EFLAGS
    iret
