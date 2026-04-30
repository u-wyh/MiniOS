section .text
global context_switch
global task_enter

context_switch:
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

    ; 首次启动任务时还没有旧上下文，因此直接切到任务栈并按中断返回格式恢复现场
    mov esp, eax
    popad
    iret

section .note.GNU-stack noalloc noexec nowrite
