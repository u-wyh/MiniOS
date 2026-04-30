# Task21：ELF Loader（用户程序加载）

## 1. 什么是 ELF

ELF（Executable and Linkable Format）是 Linux/x86 常见的可执行文件格式。内核加载程序时，需要读取 ELF 元数据，找到可加载段，然后把段映射到目标虚拟地址并拷贝内容，最后跳到入口地址执行。

本任务只实现最小静态 ELF 加载，不涉及文件系统、磁盘、动态链接和复杂重定位。

## 2. ELF Header

本轮使用 `struct Elf32_Ehdr`，重点字段：

- `e_entry`：程序入口虚拟地址
- `e_phoff`：Program Header 表偏移
- `e_phnum`：Program Header 项数量

加载前先校验：

- ELF Magic（`0x7F 'E' 'L' 'F'`）
- 32 位、小端格式
- Program Header 表长度不越界

## 3. Program Header

本轮使用 `struct Elf32_Phdr`，只处理 `p_type == PT_LOAD` 的段。

关键字段：

- `p_offset`：段在 ELF 镜像中的偏移
- `p_vaddr`：段目标虚拟地址
- `p_filesz`：文件内有效字节数
- `p_memsz`：内存占用字节数
- `p_flags`：段权限（读/写/执行）

## 4. 段加载过程

对每个 `PT_LOAD`：

1. 计算段覆盖的虚拟页区间
2. 对每页 `alloc_page()`
3. 建立映射：`map_page(va, pa, flags)`
4. 先清零 `[p_vaddr, p_vaddr + p_memsz)`
5. 拷贝文件数据到段首地址（`p_filesz` 字节）

加载完成后返回 `e_entry` 作为用户态入口。

## 5. VA -> PA 映射

映射由分页模块负责：

- `map_page` 根据虚拟地址找到 PDE/PTE
- 不存在页表时按需分配页表页
- 用户段使用 `PAGE_USER`，可写段追加 `PAGE_WRITABLE`

因此，ELF 段在用户虚拟地址连续可见，但底层由内核按页分配物理页承载。

## 6. 运行结果

`user` 命令触发后，内核执行：

1. `elf_load()` 加载内存 ELF
2. 设置 `entry = e_entry`
3. `enter_user_mode(entry, USER_STACK_TOP)`

用户程序成功进入 Ring3，并通过 `write` 系统调用输出：

`Hello from ELF`
