section .text
global context_switch

context_switch:
    ; 取出 old_esp 指针和 new_esp 值，准备切换前后两个上下文
    mov eax, [esp + 4]
    mov ebx, [esp + 8]

    ; 保存当前通用寄存器现场，保证返回时状态一致
    pushad

    ; 把当前保存现场后的栈顶写回旧任务/调度器的 esp
    mov [eax], esp

    ; 切换到新任务的栈顶
    mov esp, ebx

    ; 从新任务栈中恢复通用寄存器，然后 ret 到它的执行点
    popad
    ret

section .note.GNU-stack noalloc noexec nowrite
