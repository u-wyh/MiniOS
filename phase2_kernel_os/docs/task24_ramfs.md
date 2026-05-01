# Task24：简易文件系统（ramfs）

## 1. 什么是文件系统

文件系统是操作系统管理“文件名 -> 数据内容”的机制。它负责组织、查找和读取文件。

在完整系统中，文件系统通常工作在磁盘之上；本任务先实现最小内存版，聚焦接口和执行链路。

## 2. ramfs 原理

ramfs（RAM File System）把文件直接放在内存里，用数组保存文件表。

特点：

- 访问简单、速度快
- 不需要磁盘驱动
- 重启后数据不保留

本轮实现仅用于教学验证，不追求持久化。

## 3. 文件结构设计

本任务采用最小结构：

- `struct file { const char* name; void* data; uint32_t size; }`

其中：

- `name`：文件名
- `data`：文件数据起始地址（本轮是内嵌 ELF）
- `size`：文件总字节数

并使用 `file_table[]` 维护全部文件（hello/info/loop）。

## 4. exec 如何从文件加载

Task24 前，`exec` 从 `program_table` 找程序。

Task24 后，`exec(name)` 改为：

1. 调用 `fs_find(name)` 查找文件
2. 取到 `file->data` 与 `file->size`
3. 调用 `elf_load(file->data, file->size)`
4. 跳入用户态执行入口

这样 exec 与文件系统耦合方式更接近真实 OS。

## 5. shell 与 fs 的关系

shell 新增三条命令：

- `ls`：调用 `fs_list()`，列出文件
- `cat <file>`：调用 `fs_read(name)`，显示文件信息
- `run <file>`：调用 `exec(name)`，执行该 ELF 文件

因此 shell 变成文件系统与程序执行链路的用户入口。
