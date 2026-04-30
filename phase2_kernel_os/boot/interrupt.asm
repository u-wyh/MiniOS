section .text
global isr80
global irq0_stub
global irq1_stub
global enter_user_mode
extern interrupt_handler_80
extern timer_handler
extern keyboard_handler

USER_CODE_SEL equ 0x1B
USER_DATA_SEL equ 0x23

isr80:
    ; 进入 ISR 后先保存通用寄存器，避免破坏被中断上下文
    pusha

    ; 把当前中断现场入口传给 C 层，便于区分这次 int 0x80 来自内核还是用户态
    push esp

    ; 调用 C 层中断处理函数，执行中断逻辑
    call interrupt_handler_80

    ; 清理传入的中断现场参数
    add esp, 4

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

; 构造 Ring3 返回帧，通过 iretd 从内核主动降权到用户态执行
; 第一个参数是用户态入口地址，第二个参数是用户栈顶地址
enter_user_mode:
    mov eax, [esp + 4]
    mov edx, [esp + 8]

    ; 预先把数据段切到用户段，这样进入 Ring3 后 DS/ES/FS/GS 也是合法的 DPL=3 选择子
    mov bx, USER_DATA_SEL
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx

    push dword USER_DATA_SEL
    push edx
    pushfd
    push dword USER_CODE_SEL
    push eax
    iretd

section .note.GNU-stack noalloc noexec nowrite
