section .text
global isr80
global irq0_stub
extern interrupt_handler_80
extern timer_handler

isr80:
    ; 进入 ISR 后先保存通用寄存器，避免破坏被中断上下文
    pusha

    ; 调用 C 层中断处理函数，执行中断逻辑
    call interrupt_handler_80

    ; 恢复通用寄存器，确保返回后上下文一致
    popa

    ; 使用 iret 从中断返回，恢复 EIP/CS/EFLAGS
    iret

irq0_stub:
    ; 保存通用寄存器，避免定时器打断当前执行现场
    pusha

    ; 调用 C 层定时器处理函数，完成 tick 统计与屏幕输出
    call timer_handler

    ; 恢复通用寄存器，使中断返回后继续原流程
    popa

    ; 用 iret 恢复中断前的执行点与标志寄存器
    iret

section .note.GNU-stack noalloc noexec nowrite
