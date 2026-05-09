; interrupt.asm：提供系统调用、时钟中断、键盘中断和用户态切换的汇编入口
section .text
global isr80
global irq0_stub
global irq1_stub
global enter_user_mode
extern interrupt_handler_80
extern syscall_should_halt
extern syscall_clear_halt
extern syscall_should_idle
extern syscall_clear_idle
extern syscall_take_resume_frame
extern kernel_shell_loop
extern kernel_idle_loop
extern paging_get_kernel_virtual_base
extern stack_top
extern timer_handler
extern keyboard_handler

USER_CODE_SEL equ 0x1B
USER_DATA_SEL equ 0x23
KERNEL_DATA_SEL equ 0x10

isr80:
    ; 进入 ISR 后先保存通用寄存器，避免破坏被中断上下文
    pusha

    ; 把当前中断现场入口传给 C 层，便于区分这次 int 0x80 来自内核还是用户态
    push esp

    ; 调用 C 层中断处理函数，执行中断逻辑
    call interrupt_handler_80

    ; 清理传入的中断现场参数
    add esp, 4

    ; 若本次 syscall 需要直接切换到另一份用户态现场，则改用目标 frame 做 popa + iretd
    call syscall_take_resume_frame
    test eax, eax
    jz .check_halt_after_syscall

    mov esp, eax
    jmp .return_from_syscall

    ; 如果用户程序执行了 SYS_EXIT，则直接切回内核 shell 主循环，
    ; 不再通过 iretd 返回到用户态后续指令。
.check_halt_after_syscall:
    call syscall_should_halt
    test eax, eax
    jz .check_idle_after_syscall

    call syscall_clear_halt

    mov bx, KERNEL_DATA_SEL
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx
    mov ss, bx
    mov esp, stack_top
    call paging_get_kernel_virtual_base
    add eax, kernel_shell_loop
    jmp eax

.check_idle_after_syscall:
    call syscall_should_idle
    test eax, eax
    jz .return_from_syscall

    call syscall_clear_idle

    mov bx, KERNEL_DATA_SEL
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx
    mov ss, bx
    mov esp, stack_top
    call paging_get_kernel_virtual_base
    add eax, kernel_idle_loop
    jmp eax

    ; 恢复通用寄存器，确保返回后上下文一致
.return_from_syscall:
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
