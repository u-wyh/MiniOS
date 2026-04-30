section .text
global context_switch
global task_enter

context_switch:
    ; 这个入口只保留给“普通函数式栈切换”语义，不参与当前 PIT 中断调度路径
    ; 取出 old_esp 指针和 new_esp 值，准备切换前后两个上下文
    mov eax, [esp + 4]
    mov ebx, [esp + 8]

    ; pushad 会按固定顺序把 8 个通用寄存器压栈，形成完整软件上下文
    pushad

    ; 把当前保存现场后的栈顶写回旧任务/调度器的 esp
    mov [eax], esp

    ; 切换到新任务的栈顶
    mov esp, ebx

    ; popad 从新任务栈中按相反顺序恢复寄存器，ret 再跳到新任务保存的返回地址
    popad
    ret

task_enter:
    ; 读取首个任务准备好的中断现场栈顶地址
    mov eax, [esp + 4]

    ; 当前自动调度真正依赖的是这一条路径：首次启动就按“中断恢复模型”进入任务
    mov esp, eax
    popad
    iretd

section .note.GNU-stack noalloc noexec nowrite
