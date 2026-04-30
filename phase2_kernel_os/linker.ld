ENTRY(_start)

SECTIONS {
    /* 将内核加载到 1MB 位置，避免与 BIOS 低端内存冲突 */
    . = 1M;

    /* 保留 Multiboot 头，确保加载器可在前部区域找到 */
    .multiboot : {
        *(.multiboot)
    }

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(COMMON)
        *(.bss*)
    }

    /* 导出内核镜像结束位置，供物理内存分配器避开内核自身占用区域 */
    _kernel_end = .;
}
