section .text
global isr80
global irq0_stub
global irq1_stub
extern interrupt_handler_80
extern timer_handler
extern keyboard_handler

isr80:
    ; 进入 ISR 后先保存通用寄存器，避免破坏被中断上下文
    pusha

    ; 调用 C 层中断处理函数，执行中断逻辑
    call interrupt_handler_80

    ; 恢复通用寄存器，确保返回后上下文一致
    popa

    ; 使用 iretd 从中断返回，显式恢复 32 位 EIP/CS/EFLAGS
    iretd

irq0_stub:
    ; CPU 进入中断前已自动压入 EIP/CS/EFLAGS，这里再补上通用寄存器
    pusha

    ; 把当前中断现场的栈顶地址传给 C 层，便于调度器保存旧任务 ESP
    push esp

    ; 调用 C 层定时器处理函数，完成 tick 统计与任务切换决策
    call timer_handler

    ; 清理传入的参数，恢复到中断现场原本的栈布局
    add esp, 4

    ; 调度器返回的不是“函数返回地址”，而是新任务完整中断现场的栈顶
    mov esp, eax

    ; popa 从当前 esp 指向的任务栈中恢复 8 个通用寄存器
    popa

    ; iretd 再继续恢复 EIP/CS/EFLAGS，最终返回到被选中的任务
    iretd

irq1_stub:
    ; 保存通用寄存器，避免键盘中断破坏当前执行上下文
    pusha

    ; 调用 C 层键盘处理函数，读取扫描码并输出字符
    call keyboard_handler

    ; 恢复通用寄存器，让中断返回后继续原流程
    popa

    ; 使用 iretd 从键盘中断返回
    iretd

section .note.GNU-stack noalloc noexec nowrite
