# MiniOS Phase 2：裸机内核开发文档

## 一、阶段目标

Phase 2 目标是从用户态模拟进入裸机内核开发，构建可在 QEMU 运行的 MiniOS。

最终能力范围：

- 自定义 BootLoader
- 内核启动流程
- VGA 屏幕输出
- 中断机制（IDT）
- 定时器（PIT）
- 键盘输入
- 内存管理（分页）
- 进程调度
- 系统调用
- 简易 Shell
- 简易文件系统

## 二、开发环境

当前环境：

- Ubuntu 虚拟机
- GCC 13.x（freestanding）
- NASM
- LD
- QEMU
- GDB

编译约束：

- 不依赖标准库（libc）
- freestanding 模式
- 32 位内核（x86）

## 三、项目结构

```text
phase2_kernel_os/
├── boot/        # 启动相关（汇编）
├── drivers/     # 驱动模块（如 VGA）
├── kernel/      # 内核核心逻辑（C）
├── include/     # 头文件与模块接口
├── build/       # 构建输出
├── linker.ld    # 链接脚本
└── Makefile     # 构建系统
```

说明：

- `boot/`：启动与底层汇编入口
- `drivers/`：设备与基础输出驱动
- `kernel/`：内核初始化与核心流程
- `include/`：统一头文件接口
- `build/`：编译产物（不纳入 Git）

## 四、开发原则

### 1. 最小改动原则

每一轮只完成一个最小功能，且必须满足：

- 可编译
- 可运行
- 可验证

### 2. 阶段推进顺序

禁止跨阶段提前实现复杂子系统。推进顺序：

```text
启动 -> 输出 -> GDT/IDT -> 驱动中断 -> 内存 -> 调度 -> 系统调用 -> Shell/文件系统
```

### 3. 强制验证流程

每个任务都必须执行并通过：

```bash
make clean
make
make run
```

### 4. 工程化要求

- 使用 Makefile 管理构建
- 保持目录职责清晰
- 模块拆分、接口统一

### 5. 注释要求

新增代码应有简洁中文注释，说明作用与设计意图。

## 五、阶段进度（已完成）

### A. 基础启动阶段（Task1 + Task2）

#### Task1：裸机内核启动

实现内容：

- `boot.asm` 进入内核
- `kernel_main` 成功执行
- VGA 文本模式输出启动信息
- QEMU 成功启动内核

输出结果：

```text
MiniOS Kernel Boot Success
```

问题与修复：

- 问题：输出后残留 BIOS 内容
- 原因：未清屏
- 修复：实现 `clear_screen()`

#### Task2：VGA 输出模块化

实现内容：

- 新增 `clear_screen()`
- 新增 `print_char(char c)`（支持 `\n`）
- 新增 `print_string(const char* str)`

修改文件：

- 新增 `drivers/vga.c`
- 新增 `include/vga.h`
- 修改 `kernel/kernel.c`
- 修改 `Makefile`

结果：

- 内核输出能力从“硬编码显存写入”升级为“可复用 VGA 模块接口”

### B. 保护模式与中断阶段（Task3 + Task4）

#### Task3：GDT 最小初始化

实现内容：

- 在 `boot.asm` 新增最小 GDT（null/code/data）
- 新增 GDTR 并执行 `lgdt`
- 通过 far jump 刷新 `CS`
- 重载 `DS/ES/FS/GS/SS`

修改文件：

- 修改 `boot/boot.asm`
- 修改 `readme.md`

结果：

- 启动后不再依赖加载器默认段环境
- 已建立最小受控段模型，为 IDT/中断扩展打基础

#### Task4：IDT 与手动触发中断

实现内容：

- 建立最小 IDT（256 项）
- 注册软件中断向量 `0x80`
- 通过 `lidt` 加载 IDT
- 内核中手动触发 `int 0x80` 并返回

修改文件：

- 新增 `include/idt.h`
- 新增 `kernel/idt.c`
- 新增 `boot/interrupt.asm`
- 修改 `kernel/kernel.c`
- 修改 `Makefile`
- 修改 `readme.md`

验证结果：

- `make clean` / `make` / `make run` 通过
- QEMU 输出包含：
  - `MiniOS Kernel Boot Success`
  - `interrupt triggered`

IDT 描述符结构（8 字节）：

```text
63                              32 31                             0
+--------------------------------+--------------------------------+
| Offset 31..16 | Selector       | Type/Attr | Zero | Offset 15..0 |
+--------------------------------+--------------------------------+
```

字段说明：

- `Offset`：中断处理入口地址，被拆成低 16 位和高 16 位
- `Selector`：代码段选择子，本项目使用 GDT 代码段 `0x08`
- `Zero`：保留字节，必须为 `0`
- `Type/Attr`：门类型和属性，本项目使用 `0x8E`

当前 `0x80` 中断门对应关系：

- 入口地址：`isr80`
- 段选择子：`0x08`
- 类型属性：`0x8E`（32 位 interrupt gate，present=1，DPL=0）

结果：

- 内核具备最小中断注册与触发能力
- 已形成扩展异常/时钟/键盘中断的基础框架

## 六、当前能力状态

当前系统已具备：

- 可启动内核
- 可执行 C 代码
- 可进行模块化文本输出
- 可注册并触发最小软件中断
- 可在 QEMU 下稳定验证

已打通链路：

```text
BIOS -> Bootloader -> Kernel -> IDT/ISR(最小)
```

## 七、下一步计划

下一任务：PIT 定时器中断

- 接入 PIT 并周期触发中断
- 在 ISR 中增加最小计数逻辑
- 增加可控的调试输出（避免刷屏）

## 八、长期路线

```text
Task1 启动内核
Task2 输出系统
Task3 GDT
Task4 IDT
Task5 PIT
Task6 键盘驱动
Task7 内存管理
Task8 调度器
Task9 系统调用
Task10 Shell
```

## 九、文档维护规则

后续每轮任务完成后：

1. 更新“阶段进度（已完成）”
2. 按分组补充“实现内容 / 修改文件 / 验证结果 / 问题修复”
3. 更新“下一步计划”
4. 不删除历史记录，只做增量维护

## 十、阶段总结

MiniOS 已完成从“可启动内核”到“最小中断框架”的过渡，Phase 2 处于可持续迭代状态，可继续推进 PIT、键盘和内存管理模块。

## 附录A：分段机制学习笔记（保留）

### 1. 分段机制是什么

分段（Segmentation）是 x86 在保护模式下的内存访问规则系统。
CPU 访问内存时遵循“段描述符”定义的基址、界限和权限。

### 2. 核心组成

- GDT（Global Descriptor Table）：段规则表
- 段寄存器：`CS/DS/SS/ES/FS/GS`
- 选择子（Selector）：段寄存器中保存的索引信息

选择子结构（16 位）：

```text
15                           3 2   1 0
+-----------------------------+---+---+
|         Index               |TI |RPL|
+-----------------------------+---+---+
```

- `Index`：GDT/LDT 描述符索引
- `TI`：`0=GDT`，`1=LDT`
- `RPL`：请求特权级（0~3）

示例：

- `0x08`：代码段选择子（Index=1）
- `0x10`：数据段选择子（Index=2）

### 3. 地址转换过程

```text
逻辑地址 = selector : offset
线性地址 = base + offset
```

过程：

1. 从段寄存器取 selector
2. 根据 selector 在 GDT 中定位描述符
3. 取出 base/limit/权限
4. 检查 offset 是否越界
5. 计算线性地址

### 4. GDT 描述符结构（8 字节）

```text
63                     32 31                    0
+----------------------+------------------------+
| Base 31..24 | Flags  | Limit 19..16 | Access  |
+----------------------+------------------------+
| Base 23..16          | Base 15..0             |
+----------------------+------------------------+
| Limit 15..0                                   |
+-----------------------------------------------+
```

- `Base`：段基址
- `Limit`：段界限
- `Access`：类型与权限
- `Flags`：粒度、默认操作数大小等

### 5. 段寄存器要点

- `CS`：代码段
- `DS`：默认数据段
- `SS`：栈段
- `ES/FS/GS`：附加数据段

注意：

- 不能直接 `mov cs, ...`，需要 far jump/retf 等方式刷新 `CS`
- 重新加载 `SS` 后应保证栈访问环境一致

### 6. 分段的作用

- 内存隔离
- 越界保护
- 权限控制（代码段/数据段、ring0/ring3）

### 7. 平坦模型（Flat Model）

常见设置：

```text
base = 0
limit = 4GB
```

效果：

```text
线性地址 = offset
```

结论：现代系统常保留分段的权限语义，实际以内存分页为核心。

### 8. 当前 MiniOS 对应关系

当前已完成：

- 初始化最小 GDT
- `lgdt` 加载 GDTR
- far jump 刷新 `CS`
- 重载 `DS/ES/FS/GS/SS`

阶段意义：

- 从“依赖加载器默认段环境”进入“内核主动建立段环境”
- 为后续 IDT/PIT/分页模块打基础
