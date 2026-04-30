; Multiboot 魔数，供支持 Multiboot 的加载器识别
MB_MAGIC    equ 0x1BADB002
; 标志位：按页对齐并请求内存信息
MB_FLAGS    equ 0x00000003
; 校验和，保证三者相加为 0
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

; GDT 代码段选择子（第 1 项，偏移 0x08）
CODE_SEL    equ 0x08
; GDT 数据段选择子（第 2 项，偏移 0x10）
DATA_SEL    equ 0x10
; GDT 用户代码段选择子（第 3 项，进入 Ring3 时要带上 RPL=3）
USER_CODE_SEL equ 0x18
; GDT 用户数据段选择子（第 4 项，进入 Ring3 时要带上 RPL=3）
USER_DATA_SEL equ 0x20
; GDT TSS 选择子（第 5 项）
TSS_SEL     equ 0x28

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

section .text
global _start
extern kernel_main

_start:
    ; 加载我们自定义的 GDT，让 CPU 使用受控的段描述符集合
    lgdt [gdt_descriptor]

    ; 通过远跳转强制刷新 CS，切换到新 GDT 的代码段描述符
    jmp CODE_SEL:.reload_cs

.reload_cs:
    ; 重新加载数据相关段寄存器，确保与新 GDT 的数据段一致
    mov ax, DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; 初始化 TSS 的 Ring0 栈，让 CPU 能在用户态 int 0x80 时切回内核栈
    mov dword [tss_entry + 4], stack_top
    mov word [tss_entry + 8], DATA_SEL

    ; 运行时把 TSS 基地址写入 GDT 描述符，避免在重定位目标里直接拆符号高字节
    mov eax, tss_entry
    mov word [gdt_tss + 2], ax
    shr eax, 16
    mov byte [gdt_tss + 4], al
    mov byte [gdt_tss + 7], ah

    ; 装载任务状态段，后续从 Ring3 进入 Ring0 时会依赖它完成特权级栈切换
    mov ax, TSS_SEL
    ltr ax

    ; 初始化栈顶，保证进入 C 代码前有可用栈空间
    mov esp, stack_top
    ; 调用 C 语言内核入口
    call kernel_main

.hang:
    ; 如果 kernel_main 返回，停机等待并保持死循环
    cli
    hlt
    jmp .hang

section .data
align 8
gdt_start:
    ; 空描述符：GDT 第 0 项必须为 null descriptor
    dq 0x0000000000000000
    ; 代码段：base=0，limit=4GB，32 位，可执行可读
    dq 0x00CF9A000000FFFF
    ; 数据段：base=0，limit=4GB，32 位，可读写
    dq 0x00CF92000000FFFF
    ; 用户代码段：base=0，limit=4GB，32 位，可执行可读，DPL=3
    dq 0x00CFFA000000FFFF
    ; 用户数据段：base=0，limit=4GB，32 位，可读写，DPL=3
    dq 0x00CFF2000000FFFF
    ; TSS 描述符：描述 Ring3 -> Ring0 中断切换时使用的内核栈
gdt_tss:
    dw tss_end - tss_entry - 1
    dw 0
    db 0
    db 0x89
    db 0x00
    db 0
gdt_end:

gdt_descriptor:
    ; GDT 界限（字节数 - 1）
    dw gdt_end - gdt_start - 1
    ; GDT 基地址
    dd gdt_start

section .bss
alignb 16
tss_entry:
    ; 104 字节最小 32 位 TSS，本轮只使用 esp0/ss0 做 Ring3 -> Ring0 栈切换
    resb 104
tss_end:

alignb 16
stack_bottom:
    ; 预留 16KB 栈空间
    resb 16384
stack_top:

section .note.GNU-stack noalloc noexec nowrite
