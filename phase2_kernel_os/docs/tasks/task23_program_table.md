# Task23：多用户程序表 + run 命令扩展

## 1. 什么是 program table

Program Table 是一个最小映射表，用于把“程序名”映射到“内置 ELF 镜像地址”。

本轮结构：

- `struct program { const char* name; void* elf_data; }`

表中注册：

- `hello`
- `info`
- `loop`

## 2. 为什么不直接写死单个程序

写死单个程序只能做一次固定演示，不具备扩展性。

引入 program table 后，`exec` 不需要为每个程序写一套分支逻辑，只需要按名称查表即可扩展更多程序，成本更低，也更接近后续文件系统加载模型。

## 3. exec 如何查找程序

`exec(name)` 执行流程：

1. 遍历 `program_table`
2. 用最小字符串比较匹配 `name`
3. 找到后取出 `elf_data`
4. 计算 ELF 镜像长度
5. 调用 `elf_load(elf_data, elf_size)`
6. 通过 `enter_user_mode(entry, USER_STACK_TOP)` 执行

## 4. shell → exec → ELF loader 流程

Shell 支持 `run <name>`：

- `run hello`
- `run info`
- `run loop`

最小链路：

`shell -> exec(name) -> program_table 查找 -> elf_load -> user mode`

## 5. 为什么现在不做文件系统

本阶段目标是先打通“按名字运行多个程序”的内核执行路径。

如果提前加入文件系统，会引入路径解析、磁盘读写、格式处理等复杂度，掩盖本任务重点。当前使用“内嵌 ELF”能保证改动小、验证直接。

## 6. 程序运行流程图

```text
+---------+      +-----------+      +----------------+      +----------+      +-----------+
| shell   | ---> | exec(name)| ---> | program_table  | ---> | elf_load | ---> | user mode |
| run xxx |      |           |      | name -> elf    |      |          |      | entry     |
+---------+      +-----------+      +----------------+      +----------+      +-----------+
```
