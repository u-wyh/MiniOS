; Multiboot 魔数，供支持 Multiboot 的加载器识别
MB_MAGIC    equ 0x1BADB002
; 标志位：按页对齐并请求内存信息
MB_FLAGS    equ 0x00000003
; 校验和，保证三者相加为 0
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

section .text
global _start
extern kernel_main

_start:
    ; 初始化栈顶，保证进入 C 代码前有可用栈空间
    mov esp, stack_top
    ; 调用 C 语言内核入口
    call kernel_main

.hang:
    ; 如果 kernel_main 返回，停机等待并保持死循环
    cli
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    ; 预留 16KB 栈空间
    resb 16384
stack_top:
