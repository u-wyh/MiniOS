# MiniOS Phase 2：裸机内核开发文档

---

# 📌 一、阶段目标

Phase 2 的目标是：

> 从用户态模拟操作系统，进入真正的“裸机内核开发”，构建一个可以在 QEMU 上运行的 MiniOS。

最终成果应包括：

* 自定义 BootLoader
* 内核启动流程
* 屏幕输出（VGA）
* 中断机制（IDT）
* 定时器（PIT）
* 键盘输入
* 内存管理（分页）
* 进程调度
* 系统调用
* 简易 Shell
* 简易文件系统

---

# 📌 二、开发环境

当前开发环境：

* Ubuntu 虚拟机（主开发环境）
* GCC 13.x（freestanding 编译）
* NASM（汇编）
* LD（链接）
* QEMU（系统运行）
* GDB（调试）

编译特点：

* 不依赖标准库（libc）
* freestanding 模式
* 32 位内核（x86）

---

# 📌 三、项目结构

当前目录结构：

```
phase2_kernel_os/
├── boot/        # 启动相关（汇编）
├── kernel/      # 内核核心逻辑（C）
├── include/     # 头文件
├── build/       # 编译输出
├── linker.ld    # 链接脚本
└── Makefile     # 构建系统
```

说明：

* boot：负责从启动到进入内核
* kernel：核心逻辑入口
* include：后续模块统一接口
* build：不纳入 git
* linker.ld：控制内存布局

---

# 📌 四、开发原则（非常重要）

本项目遵循：

## 1. 最小改动原则

每一轮只做一个最小功能，确保：

* 可编译
* 可运行
* 可验证

## 2. 阶段推进

禁止一次实现复杂系统：

❌ 不允许直接写调度器
❌ 不允许提前写文件系统

必须：

```text
启动 → 输出 → 中断 → 驱动 → 内存 → 调度
```

## 3. 强制验证

每个任务必须满足：

```bash
make clean
make
make run
```

并在 QEMU 中看到正确结果。

## 4. 工程化开发

* 使用 Makefile
* 目录清晰
* 模块拆分
* 统一接口

## 5. 中文注释要求

所有新增代码必须：

```text
添加中文注释，说明作用和设计意图
```

---

# 📌 五、已完成任务

## ✅ Task1：裸机内核启动

### 功能

* boot.asm 正确进入内核
* kernel_main 成功执行
* VGA 文本模式输出
* QEMU 成功启动内核

### 输出结果

```
MiniOS Kernel Boot Success
```

### 技术点

* Multiboot / 启动入口
* freestanding 编译
* VGA 显存（0xB8000）
* 内核入口函数调用
* Makefile 构建流程

### 问题与修复

问题：

```
输出字符串后残留 BIOS 内容
```

原因：

```
未清屏
```

解决：

```
实现 clear_screen()
```

---

# 📌 六、当前能力状态

当前系统具备：

```text
✔ 可启动内核
✔ 可执行 C 代码
✔ 可输出文本到屏幕
✔ 可使用 QEMU 运行
```

这意味着：

```text
BIOS → Bootloader → Kernel 链路已打通
```

---

# 📌 七、下一步计划

## Task2：VGA 输出模块化

目标：

实现基础输出接口：

```c
clear_screen()
print_char()
print_string()
```

作用：

* 后续调试基础
* 内核日志输出
* panic 输出
* 中断调试

---

# 📌 八、长期路线

Phase2 总体推进路径：

```
Task1 启动内核
Task2 输出系统
Task3 GDT（保护模式）
Task4 IDT（中断）
Task5 PIT（时钟）
Task6 键盘驱动
Task7 内存管理
Task8 调度器
Task9 系统调用
Task10 Shell
```

---

# 📌 九、后续文档维护规则（给 Codex 用）

后续每一轮任务完成后，必须：

1. 更新“已完成任务”部分
2. 补充：

   * 新增功能
   * 修改文件
   * 遇到问题
   * 解决方案
3. 更新“下一步计划”
4. 不删除历史记录，只追加

---

# 📌 十、当前阶段总结

MiniOS 已正式进入：

```text
裸机内核开发阶段
```

并完成：

```text
系统启动能力
```

这是整个操作系统开发中：

```text
最关键的第一步
```

---

---

## ✅ Task2：VGA 输出模块

### 新增功能

- 新增 `clear_screen()`：清空 80x25 VGA 文本屏幕
- 新增 `print_char(char c)`：输出单字符，支持 `\n` 并自动移动光标
- 新增 `print_string(const char* str)`：循环调用 `print_char` 输出字符串

### 修改文件列表

- 新增：`drivers/vga.c`
- 新增：`include/vga.h`
- 修改：`kernel/kernel.c`
- 修改：`Makefile`

### 技术点

- 使用 VGA 文本显存地址 `0xB8000`
- 采用固定屏幕尺寸 `80x25`
- 使用默认颜色属性 `0x0F`
- 维护最小光标状态：`row` 与 `col`

### 当前系统能力提升

- `kernel_main` 已与显存硬编码解耦，改为调用 VGA 模块接口
- 内核输出能力从“单点写显存”升级为“可复用输出模块”
