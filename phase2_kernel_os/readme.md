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

#### Task5：PIT 定时器中断

本轮目标：

- 重映射 PIC，避免 IRQ0 与 CPU 异常向量冲突
- 注册 `IRQ0 -> IDT[0x20]`
- 初始化 PIT，让硬件周期性触发定时器中断
- 在中断处理函数中输出 `T`，验证硬件中断持续发生

新增功能：

- 新增端口 IO 封装：`outb()` / `inb()` / `io_wait()`
- 新增 PIC 重映射与 `EOI` 发送逻辑
- 新增 PIT 初始化与 `timer_handler()`
- 新增 `irq0_stub`，将硬件中断接到 C 层处理函数
- 在 `kernel_main` 中完成 `IDT -> PIC -> PIT -> sti` 初始化顺序

修改文件：

- 新增 `include/io.h`
- 新增 `kernel/io.c`
- 新增 `include/pic.h`
- 新增 `kernel/pic.c`
- 新增 `include/pit.h`
- 新增 `kernel/pit.c`
- 修改 `include/idt.h`
- 修改 `kernel/idt.c`
- 修改 `boot/interrupt.asm`
- 修改 `kernel/kernel.c`
- 修改 `Makefile`
- 修改 `readme.md`

技术点：

- PIT：8253/8254 可编程定时器，本轮使用通道 0 周期触发 IRQ0
- PIC：8259A 可编程中断控制器，本轮先重映射到 `0x20~0x2F`
- IRQ0：来自 PIT 的定时器硬件中断，重映射后对应 `IDT[0x20]`
- EOI：中断处理结束后必须发送给 PIC，否则后续 IRQ0 会停止
- `sti / hlt`：`sti` 开启中断，`hlt` 让 CPU 在空闲时等待下一次中断

相关知识点：

- `IRQ`（Interrupt Request）表示硬件设备发起的中断请求；本轮的 `IRQ0` 来自 PIT 定时器
- PIC 默认会把 `IRQ0~IRQ7` 放到 `0x08~0x0F`，这会与 CPU 异常向量冲突，所以必须先重映射
- PIC 重映射后，主片通常对应 `0x20~0x27`，从片对应 `0x28~0x2F`，这样 `IRQ0` 就落到 `0x20`
- PIT 通过向端口 `0x43` 写控制字、向端口 `0x40` 写分频值，周期性地产生时钟节拍
- CPU 收到 `IRQ0` 后，会通过 `IDT[0x20]` 找到 `irq0_stub`，再进入 C 层 `timer_handler()`
- 中断处理完成后必须发送 `EOI`（End Of Interrupt）给 PIC，意思是“这次 IRQ 已处理完，可以继续发送下一次中断”
- 如果不发送 `EOI`，常见现象就是只出现一个 `T`，随后定时器中断停止
- `sti` 用于打开中断允许位；如果不执行它，即使 IDT、PIC、PIT 都初始化好了，硬件中断也不会真正进入 CPU
- `hlt` 让 CPU 在空闲时睡眠等待下一次中断；下一次 PIT 中断到来时，CPU 会被唤醒并继续执行中断处理流程

PIC 基础结构：

- PIC（Programmable Interrupt Controller）是可编程中断控制器，经典 x86 上通常是 8259A 兼容模型
- 它分为主片和从片：主片管理 `IRQ0~IRQ7`，从片管理 `IRQ8~IRQ15`
- 从片通过主片的 `IRQ2` 级联到 CPU，所以处理中断时常会区分“来自主片还是从片”
- 本轮只使用主片上的 `IRQ0`，因此 `EOI` 只需要发给主片；若以后处理中断号 `>= 8`，则要先给从片发 `EOI`，再给主片发

PIT 基础结构：

- PIT（Programmable Interval Timer）是可编程定时器，经典芯片是 8253/8254
- 它有 3 个通道，本轮使用通道 0，因为通道 0 默认连接到系统定时中断 `IRQ0`
- PIT 输入基准频率约为 `1193182 Hz`，程序通过设置分频值 `divisor = 1193182 / frequency` 来得到目标中断频率
- 控制字写入端口 `0x43`，通道 0 的计数初值低字节和高字节写入端口 `0x40`
- 本轮选择较低频率，是为了既能观察到周期性中断，又不会让屏幕输出 `T` 刷得过快

验证结果：

- `make clean` / `make` / `make run` 通过
- QEMU 启动后保留 Task4 的 `interrupt triggered`
- `sti` 后 PIT 会持续触发，屏幕继续输出 `T`

当前系统能力提升：

- 内核已从“可手动触发软件中断”升级为“可处理最小硬件时钟中断”
- 已具备后续键盘输入、tick 计时和调度器的基础中断能力

下一步计划：

- 键盘中断输入

#### Task6：键盘中断输入

本轮目标：

- 注册 `IRQ1 -> IDT[0x21]`
- 通过键盘中断读取扫描码
- 将最小范围的 `a-z`/`0-9` 映射成字符并输出到 VGA

新增功能：

- 新增键盘中断汇编入口 `irq1_stub`
- 新增 `keyboard_handler()`，读取端口 `0x60`
- 新增最小 `scancode -> ASCII` 映射
- 为键盘中断增加 `pic_send_eoi(1)`

修改文件：

- 新增 `include/keyboard.h`
- 新增 `kernel/keyboard.c`
- 修改 `kernel/idt.c`
- 修改 `boot/interrupt.asm`
- 修改 `kernel/pic.c`
- 修改 `kernel/kernel.c`
- 修改 `Makefile`
- 修改 `readme.md`
- 新增 `docs/task6_keyboard.md`

验证结果：

- `make clean` / `make` / `make run` 通过
- PIT 仍可持续输出 `T`
- 按下普通字母键后，屏幕可显示对应小写字符

当前系统能力提升：

- MiniOS 已从“可自动运行并接收时钟中断”提升为“可通过键盘进行最小交互输入”

下一步计划：

- 输入缓冲区或命令行雏形

#### Task7：输入缓冲区（行输入）

本轮目标：

- 为键盘输入增加最小缓冲区
- 让系统支持“输入一行字符串 + Enter 确认”

新增功能：

- 在 `keyboard.c` 中新增 `input_buffer[128]`
- 用 `input_index` 记录当前输入位置
- 普通字符会写入 buffer 并实时回显
- Enter 会补 `'\0'`、换行、输出整行内容并清空 buffer

修改文件：

- 修改 `kernel/keyboard.c`
- 修改 `readme.md`
- 新增 `docs/task7_input.md`

验证结果：

- `make clean` / `make` / `make run` 通过
- PIT 仍然持续输出 `T`
- 输入 `a b c` 后屏幕先显示 `abc`
- 按 Enter 后会换行并再次输出 `abc`

当前系统能力提升：

- MiniOS 已从“字符输入”升级为“字符串输入”

下一步计划：

- Mini Shell

#### Task8：Mini Shell 命令解析框架

本轮目标：

- 把“输入一行字符串”升级为“输入命令并执行”
- 在启动后显示 `MiniOS> ` 提示符

新增功能：

- 新增 `shell_init()`
- 新增 `shell_execute(const char* line)`
- 支持最小命令：`help`、`clear`、`echo`
- 支持未知命令提示 `Unknown command`

修改文件：

- 新增 `include/shell.h`
- 新增 `kernel/shell.c`
- 修改 `kernel/kernel.c`
- 修改 `kernel/keyboard.c`
- 修改 `kernel/pit.c`
- 修改 `Makefile`
- 修改 `readme.md`
- 新增 `docs/task8_shell.md`

支持命令：

- `help`
- `clear`
- `echo`

验证结果：

- `make clean` / `make` / `make run` 通过
- 启动后显示 `MiniOS> `
- `help` 可显示命令列表
- `clear` 可清屏并重新显示提示符
- `echo hello` 可输出 `hello`

当前系统能力提升：

- MiniOS 已从“行输入”升级为“命令执行”

下一步计划：

- 内核命令扩展
- 或任务管理雏形

#### Task9：内核控制命令扩展

本轮目标：

- 让控制台命令开始驱动真实内核状态观察与控制

新增命令：

- `about`
- `tick`
- `panic`

修改文件：

- 新增 `include/panic.h`
- 新增 `kernel/panic.c`
- 修改 `include/pit.h`
- 修改 `kernel/pit.c`
- 修改 `kernel/shell.c`
- 修改 `Makefile`
- 修改 `readme.md`
- 新增 `docs/task9_kernel_commands.md`

验证结果：

- `help` 已包含 `about / tick / panic`
- `about` 可显示内核基本信息
- `tick` 可显示真实 PIT tick 计数
- `panic` 可输出 panic 信息并停止系统

当前系统能力提升：

- 控制台从“命令入口”升级为“内核状态观察与控制工具”

下一步计划：

- 任务管理雏形
- 或内存状态命令

#### Task10-1：任务系统雏形

本轮目标：

- 新增最小任务结构
- 实现手动任务切换

修改文件：

- 新增 `include/task.h`
- 新增 `kernel/task.c`
- 新增 `boot/switch.asm`
- 修改 `kernel/shell.c`
- 修改 `kernel/kernel.c`
- 修改 `Makefile`
- 修改 `readme.md`
- 新增 `docs/task10_task_basic.md`

验证结果：

- 输入 `task` 可在两个演示任务之间切换
- 任务 A 输出 `A`
- 任务 B 输出 `B`

当前系统能力提升：

- 已具备最小多任务切换基础（手动）

#### Task10-2：自动调度器

本轮目标：

- 使用 PIT 定时器中断自动驱动任务切换
- 在两个演示任务之间实现最小 round-robin 轮转

修改文件：

- 修改 `boot/interrupt.asm`
- 修改 `boot/switch.asm`
- 修改 `include/pit.h`
- 修改 `include/task.h`
- 新增 `include/sched.h`
- 修改 `kernel/pit.c`
- 修改 `kernel/task.c`
- 新增 `kernel/sched.c`
- 修改 `kernel/kernel.c`
- 修改 `kernel/shell.c`
- 修改 `Makefile`
- 修改 `readme.md`
- 新增 `docs/task10_sched.md`

验证结果：

- `make clean` / `make` / `make run` 通过
- 启动后无需输入 `task` 命令
- 屏幕会自动出现 `ABABAB...`

当前系统能力提升：

- 已从“手动切换任务”升级为“PIT 驱动的最小自动调度”

#### Task11：上下文切换完善

本轮目标：

- 完善 `context_switch` 的完整寄存器保存与恢复语义
- 明确任务栈中的上下文布局，提升任务切换稳定性

修改文件：

- 修改 `boot/switch.asm`
- 修改 `kernel/task.c`
- 修改 `readme.md`
- 新增 `docs/task11_ctx.md`

验证结果：

- `make clean` / `make` / `make run` 通过
- 自动调度下仍可稳定输出 `ABABAB...`
- 未出现卡死、重启或随机字符

当前系统能力提升：

- 任务切换从“能工作”进一步收紧为“上下文布局清晰、寄存器恢复明确”

#### Task12：中断调度现场统一

本轮目标：

- 统一当前 PIT 自动调度所依赖的现场模型
- 明确 CPU 自动压栈、`pusha/popa`、TCB 保存 `esp`、`iretd` 返回之间的关系

修改文件：

- 修改 `boot/interrupt.asm`
- 修改 `boot/switch.asm`
- 修改 `kernel/task.c`
- 修改 `kernel/pit.c`
- 修改 `kernel/sched.c`
- 修改 `readme.md`
- 新增 `docs/task12_interrupt_schedule.md`

中断现场保存方式：

- CPU 进入 IRQ0 时自动压入 `EIP / CS / EFLAGS`
- `irq0_stub` 再用 `pusha` 保存 8 个通用寄存器
- 调度器只需要保存当前任务 `esp`

为什么 TCB 只保存 `esp`：

- 完整现场已经保存在任务自己的栈里
- `esp` 只是这份完整现场的入口地址

当前调度路径：

```text
PIT IRQ0 -> CPU 自动压栈 -> irq0_stub: pusha -> timer_handler -> schedule
-> 保存 old esp / 切换 new esp -> popa -> iretd -> 新任务继续执行
```

验证结果：

- `make clean` / `make` / `make run` 通过
- PIT 自动调度仍稳定输出 `ABABAB...`
- 未出现 triple fault 或随机重启

下一步计划：

- 内存管理雏形
- 或任务状态管理

#### Task13：物理内存管理（页分配器）

本轮目标：

- 实现最小物理页分配器
- 支持页分配、释放与统计
- 通过 shell 命令验证内存资源分配能力

修改文件：

- 新增 `include/mm.h`
- 新增 `kernel/mm.c`
- 修改 `kernel/kernel.c`
- 修改 `kernel/shell.c`
- 修改 `Makefile`
- 修改 `readme.md`
- 新增 `docs/task13_mm.md`

验证方法：

- `mem`：查看总页数、已用页数、空闲页数
- `alloc`：分配一个 4KB 页并打印地址
- `free`：释放最近一次分配的页

当前能力提升：

- 内核已具备最小资源分配能力，可管理固定大小的物理页

下一步计划：

- 虚拟内存雏形
- 或更高层的 `kmalloc`

#### Task14：内核内存分配器

本轮目标：

- 在页分配器之上实现最小 `kmalloc / kfree`
- 支持小块内核内存分配，而不是只能按 4KB 页整块分配

修改文件：

- 修改 `include/mm.h`
- 修改 `kernel/mm.c`
- 修改 `kernel/shell.c`
- 修改 `readme.md`
- 新增 `docs/task14_kmalloc.md`

验证方法：

- `kmalloc`：连续分配多个 32B 小块，地址应不同
- `kfree`：释放后再次 `kmalloc`，可观察地址被重复利用

当前能力提升：

- 内核已经具备最小“小块内存分配”能力，不再只能直接向页分配器要整页
- 页分配器会自动避开内核镜像自身占用的物理区域，减少自覆盖风险

下一步计划：

- 更灵活的 `kmalloc`
- 或分页 / 虚拟内存

#### Task15：分页机制

本轮目标：

- 建立最小页目录与页表
- 开启 CPU 分页机制
- 对内核早期运行区域做 identity mapping

修改文件：

- 新增 `include/paging.h`
- 新增 `kernel/paging.c`
- 修改 `kernel/kernel.c`
- 修改 `Makefile`
- 修改 `readme.md`
- 新增 `docs/task15_paging.md`

验证方法：

- 执行 `make run`
- 系统在开启分页后仍能正常启动、不中途重启
- 可通过 `paging` 命令查看分页是否启用，以及页目录/页表地址

当前能力提升：

- 内核已经具备最小地址翻译能力，开始从“直接物理地址访问”过渡到“分页管理”

下一步计划：

- 虚拟内存扩展
- 或缺页异常与更完整内存管理

#### Task16：高地址内核

本轮目标：

- 在保留低地址 identity mapping 的同时，增加 `0xC0000000` 高地址内核别名映射
- 开启分页后显式跳到高地址别名继续执行

修改文件：

- 修改 `kernel/paging.c`
- 修改 `kernel/kernel.c`
- 修改 `kernel/shell.c`
- 修改 `readme.md`
- 新增 `docs/task16_higher_half.md`

验证方法：

- `make run` 后系统仍稳定运行
- `paging` 命令可看到 `kernel high base: 0xC0000000`

当前能力提升：

- 内核已经具备最小高地址映射基础，开始把“内核地址空间”和低地址区域区分开来

下一步计划：

- 更完整的高地址链接布局
- 或用户空间 / 内核空间进一步划分

## 六、当前能力状态

当前系统已具备：

- 可启动内核
- 可执行 C 代码
- 可进行模块化文本输出
- 可注册并触发最小软件中断
- 可接收 PIT 产生的周期性硬件中断
- 可在 QEMU 下稳定验证

已打通链路：

```text
BIOS -> Bootloader -> Kernel -> IDT/ISR -> PIC/PIT
```

## 七、下一步计划

下一任务：任务系统继续演进

- 扩展调度器策略
- 引入更明确的时间片管理
- 为后续任务管理和内存管理做准备

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

## ✅ Task17：用户态 + 系统调用

本轮目标：

- 为 GDT 增加 Ring3 用户代码段和用户数据段
- 通过 `iret` 从内核主动切换到用户态执行
- 让用户态通过 `int 0x80` 进入内核
- 在内核态处理中断并打印验证信息

本轮新增能力：

- 内核已具备最小 Ring3 运行能力
- `int 0x80` 已允许用户态触发
- TSS 已提供 Ring3 -> Ring0 中断切换所需的内核栈

修改文件：

- `boot/boot.asm`
- `boot/interrupt.asm`
- `kernel/idt.c`
- `kernel/kernel.c`
- `kernel/paging.c`
- `docs/task17_user_mode.md`

验证结果：

- 系统可正常启动
- 内核会打印 `Switching to Ring3 user mode...`
- 用户态执行 `int 0x80` 后，内核打印 `syscall entered kernel from Ring3`
- QEMU 寄存器验证显示：系统调用处理后 CPU 留在 Ring0 停机，链路稳定无重启

当前系统能力提升：

- 从“只有内核态”升级为“已能进入用户态并触发最小系统调用”
- 为后续用户程序加载、参数传递、系统调用表打下基础

下一步计划：

- 系统调用参数传递
- 最小用户程序管理
- 用户态与内核态更清晰的内存隔离

## ✅ Task18：用户空间与分页完善

本轮目标：

- 为用户态建立明确的虚拟地址布局
- 显式映射用户代码页和用户栈页
- 保证用户页带 `PAGE_USER` 权限位
- 保持内核高地址映射为 supervisor-only
- 通过 shell 命令触发一次用户态 syscall 测试

新增能力：

- 用户代码区：`0x00400000`
- 用户栈顶：`0x00800000`
- 用户栈页：`0x007FF000 ~ 0x007FFFFF`
- `user` 命令可触发一次 Ring3 测试

修改文件：

- `include/paging.h`
- `kernel/paging.c`
- `include/user.h`
- `kernel/user.c`
- `kernel/kernel.c`
- `kernel/shell.c`
- `docs/task18_user_space.md`

验证结果：

- 系统正常启动到 `MiniOS>` 提示符
- 输入 `user` 后，屏幕依次显示：
  - `enter user mode...`
  - `user mode running`
  - `syscall from user mode`
- QEMU 调试寄存器验证显示：用户态可进入 Ring3，之后通过 `int 0x80` 回到 Ring0

当前系统能力提升：

- 用户态不再直接复用“随便一个可运行地址”，而是拥有明确规划的用户代码区和用户栈区
- 分页权限开始体现“用户页”和“内核页”的差异
- 高地址内核具备了最小的保护意识基础

下一步计划：

- 系统调用表
- 最小 `write` syscall
- 用户程序加载与更清晰的地址空间管理

## ✅ Task19：系统调用（write）

本轮目标：

- 把现有 `int 0x80` 演示链路收口成真正的最小 syscall 入口
- 约定系统调用号 `SYS_WRITE = 1`
- 让用户态通过寄存器传参，把字符串交给内核输出

新增能力：

- 用户态可执行：
  - `eax = SYS_WRITE`
  - `ebx = str_ptr`
  - `int 0x80`
- 内核可根据 `eax` 分发最小 `write` 系统调用
- 内核可读取用户态传入的字符串地址并输出到 VGA

修改文件：

- `include/syscall.h`
- `kernel/syscall.c`
- `kernel/idt.c`
- `kernel/user.c`
- `Makefile`
- `docs/task19_syscall.md`

验证结果：

- 系统正常启动到 `MiniOS>`
- 输入 `user` 后，用户态触发 `SYS_WRITE`
- 屏幕输出：
  - `enter user mode...`
  - `Hello from user`

当前系统能力提升：

- 用户态不再只是“能进内核”，而是已经能通过标准化入口请求一个最小内核服务
- `int 0x80` 开始具备 syscall 语义，而不只是验证消息输出

下一步计划：

- 扩展 syscall 编号
- 最小 syscall 表
- 更安全的用户指针检查

## ✅ Task20：系统调用层

本轮目标：

- 把单个 `write` syscall 扩展成最小 syscall layer
- 统一通过 `eax` 传 syscall 编号
- 支持最小返回值语义

当前支持的 syscall：

- `SYS_WRITE = 1`
- `SYS_EXIT = 2`
- `SYS_GETPID = 3`
- `SYS_TIME = 4`

参数与返回值约定：

- `eax`：syscall 编号
- `ebx`：第一个参数（当前 `write` 用它传字符串地址）
- `eax`：内核返回值

本轮验证用户态程序会依次执行：

1. `SYS_WRITE`
2. `SYS_GETPID`
3. `SYS_TIME`
4. `SYS_EXIT`

验证结果：

- 系统正常启动到 `MiniOS>`
- 输入 `user` 后，屏幕依次输出：
  - `enter user mode...`
  - `Hello from user`
  - `pid: 1`
  - `time: <tick>`
  - `user exit`

当前系统能力提升：

- `int 0x80` 已经不再只是单功能演示，而是具备了最小 syscall 分发层
- 用户态可通过统一入口请求多种内核服务
- 返回值已开始通过 `eax` 传回

修改文件：

- `include/syscall.h`
- `kernel/syscall.c`
- `kernel/idt.c`
- `kernel/user.c`
- `kernel/pit.c`
- `kernel/pic.c`
- `kernel/sched.c`
- `include/sched.h`
- `Makefile`
- `docs/task20_syscall_layer.md`

下一步计划：

- 引入真正的 syscall 表
- 增加用户指针检查
- 让 `exit` 返回控制台而不是停机收口

### Task21：ELF Loader（用户程序加载）

本任务实现最小 ELF Loader：

- 从内存中的 ELF 数组读取 `Elf32_Ehdr`
- 解析 `e_entry / e_phoff / e_phnum`
- 遍历 `Elf32_Phdr`，筛选 `PT_LOAD`
- 为段按页分配物理页并建立 `VA -> PA` 映射
- 拷贝段内容并处理 `memsz > filesz` 的零填充
- 通过 `enter_user_mode(entry, user_stack_top)` 进入 Ring3 执行 ELF 入口

验证目标：用户态程序可通过 `int 0x80` 调用 `write`，输出 `Hello from ELF`。

### Task22：exec 机制（shell 运行用户程序）

本任务新增最小 exec 链路：

- 新增 `exec(const char* name)`
- 维护最小程序分派（当前仅支持 `test`）
- shell 新增命令 `run test`
- 执行路径：`shell -> exec -> elf_load -> user mode`

验证目标：

在 shell 输入 `run test`，用户程序成功输出 `Hello from ELF`。

## Task23：多用户程序支持

本任务在不引入文件系统的前提下，增加最小 Program Table 机制：

- 新增 program table（程序名 -> 内置 ELF）
- exec 从“写死单程序”升级为“按名字查表加载”
- shell 新增 `run <name>` 命令扩展
- 当前支持：`run hello` / `run info` / `run loop`

当前能力：
MiniOS 可以运行多个用户程序。

下一步：
文件系统（程序从外部加载）

## Task24：ramfs 文件系统

本任务实现最小内存文件系统（ramfs），不依赖磁盘：

- 新增 `struct file { name, data, size }`
- 用数组维护最小文件表（hello/info/loop）
- 提供 `fs_find / fs_list / fs_read`
- shell 新增 `ls`、`cat <file>`、`run <file>`
- exec 从 program_table 改为 `fs_find(name)` 后加载 ELF

当前能力：
MiniOS 可在内存文件系统中查找并执行 ELF 文件。

## Task25：进程模型

本任务引入最小 Process/PCB 模型：

- 新增 PCB：`pid/state/esp/eip`
- 新增进程状态：`READY/RUNNING/EXIT`
- 新增 `process_create(name)`：分配 PID、加载 ELF、初始化寄存器入口
- `exec` 改为通过 `process_create` + `process_run` 执行
- shell 新增 `ps` 查看进程列表
- `SYS_GETPID` 改为返回当前进程 pid

当前能力：
用户程序可在最小 PCB 模型中运行并被 `ps` 观察。

## ✅ Task26：进程生命周期管理

本轮目标：

- 让进程从“能创建和运行”升级为“能退出、进入 ZOMBIE、再由 wait 回收”。

新增能力：

- 新增 `PROCESS_UNUSED / PROCESS_READY / PROCESS_RUNNING / PROCESS_ZOMBIE` 状态。
- 新增 `process_exit(status)`，保存退出码并把当前进程标记为 ZOMBIE。
- 新增 `process_wait()`，回收一个 ZOMBIE PCB 槽位。
- `SYS_EXIT` 使用 `ebx` 作为退出码，并调用 `process_exit`。
- shell 新增 `wait` 命令。
- `ps` 能显示 `READY / RUNNING / ZOMBIE` 与退出状态。

修改文件：

- `include/process.h`
- `kernel/process.c`
- `include/syscall.h`
- `kernel/syscall.c`
- `kernel/shell.c`
- `kernel/fs.c`
- `kernel/user.c`
- `kernel/elf.c`
- `readme.md`
- `docs/task26_process_lifecycle.md`

验证结果：

- `make clean`
- `make`
- `make run`
- `run hello`
- `ps`
- `wait`
- `ps`

当前系统能力提升：

- MiniOS 从“能创建进程”升级为“能管理最小进程生命周期”。

下一步计划：

- 增加父子进程关系。
- 实现 fork 雏形。
- 完善页表、用户栈等资源释放。

## ✅ Task27：父子进程关系与 waitpid 雏形

本轮目标：

- 让 MiniOS 从“任意回收 ZOMBIE”升级为“按父子关系回收子进程”。

新增能力：

- PCB 新增 `parent_pid`，记录进程创建者。
- `process_create(name)` 会在创建进程时保存父进程 PID。
- shell / kernel monitor 创建的进程暂时使用 `parent_pid = 0`。
- `process_wait()` 只回收当前父进程名下的 ZOMBIE 子进程。
- 新增 `process_waitpid(pid)`，支持按指定 PID 尝试回收子进程。
- shell 新增 `waitpid <pid>` 命令。
- `ps` 输出升级为 `PID / PPID / STATE / STATUS / NAME`。

修改文件：

- `include/process.h`
- `kernel/process.c`
- `kernel/shell.c`
- `readme.md`
- `docs/task27_parent_waitpid.md`

验证结果：

- `make clean`
- `make`
- `make run`
- `run hello`
- `ps`
- `wait`
- `run hello`
- `ps`
- `waitpid <pid>`
- `waitpid 999`

当前系统能力提升：

- MiniOS 从“全局扫描并回收任意 zombie”升级为“按照父子归属关系管理进程退出记录”。

下一步计划：

- 实现 fork 雏形。
- 引入真正的 init 进程。
- 完善进程资源释放。

## ✅ Task28：进程资源释放完善

本轮目标：

- 在 wait/waitpid 回收 ZOMBIE 子进程时，释放该进程占用的最小用户态资源，而不是只清 PCB 状态。

已完成：

- PCB 新增最小资源记录字段：用户栈页、ELF 映射页信息。
- ELF Loader 新增装载信息输出接口，用于记录本次装载出的用户页。
- 新增最小页解除映射接口 `unmap_page`，供进程回收阶段使用。
- 在 wait/waitpid 回收路径执行资源释放：先释放用户页，再回收 PCB。
- 回收后清空资源字段，避免重复释放。
- 回收后 PCB 恢复 `UNUSED`，可继续复用。

修改文件：

- `include/elf.h`
- `kernel/elf.c`
- `include/paging.h`
- `kernel/paging.c`
- `include/process.h`
- `kernel/process.c`
- `readme.md`
- `docs/task28_process_reclaim.md`

验证结果（最小场景）：

- `run hello` 后可正常进入 `ZOMBIE`。
- `waitpid 1` 可回收并返回 pid。
- `waitpid 999` 返回不存在进程提示。
- 连续两轮 `run hello` + 回收后仍可继续创建和回收。

当前能力提升：

- MiniOS 从“回收 PCB 记录”升级为“回收 PCB + 释放最小用户页资源”。

TODO：

- 当前仍未实现完整页表对象释放。
- 当前仍未实现 fork、COW、VMA、文件描述符级资源回收。

## ✅ Task29：exec 资源替换语义整理

本轮目标：

- 把“创建进程对象”和“装载/替换用户程序镜像”的职责进一步拆开，为后续 `fork + exec` 做准备。

已完成：

- 新增 `process_exec(proc, elf_data, elf_size)`，集中处理 ELF 装载、用户栈建立、`eip/esp` 写回。
- 新增 `process_exec_file(proc, name)`，统一处理“按文件名查程序”与“首次装载/重复 exec”入口。
- 把 Task28 的用户资源释放逻辑整理为镜像级释放入口，供 `wait/waitpid` 和未来重复 `exec` 复用。
- `process_create(name)` 现在优先负责 PCB、pid、parent_pid 创建，再调用 exec 装载镜像。
- exec 成功后，PCB 统一记录入口地址、用户栈和 ELF 用户页范围。
- 启动流程改为按需准备旧的 `user` 测试镜像，避免内核启动时提前占用 exec 使用的用户地址空间。

修改文件：

- `include/process.h`
- `kernel/process.c`
- `kernel/kernel.c`
- `readme.md`
- `docs/task25_process.md`
- `docs/task29_exec_semantics.md`

验证结果（最小场景）：

- `run hello` 仍可正常创建进程、运行用户态并退出。
- `waitpid <pid>` 仍可回收 `ZOMBIE` 子进程并释放用户镜像资源。
- 连续多次 `run hello` / `run info` + `waitpid` 后，进程槽位可继续复用。
- `run missing` 可稳定返回失败，不会导致内核崩溃。

当前能力提升：

- MiniOS 已经初步区分“创建进程对象”和“替换用户程序镜像”。
- 为后续同一进程重复 exec 的资源释放语义预留了统一入口。

TODO：

- 暂不支持 `argv/envp`。
- 暂不支持 PATH 搜索。
- 暂不支持完整 exec 失败回滚。
- 暂不支持 fork、copy-on-write、复杂 VMA。

## ✅ Task30：fork 雏形

本轮目标：

- 实现一个教学版最小 fork：父进程复制当前用户态执行现场，子进程从同一位置继续执行，并通过 waitpid 形成最小回收闭环。

已完成：

- 新增 `SYS_FORK` 和 `SYS_WAITPID` 系统调用入口。
- fork 后父进程返回子进程 pid，子进程返回 0。
- 子进程拥有独立 PCB，并正确记录 `parent_pid`。
- 在共享页表模型下，新增按进程重新安装用户页映射的最小切换逻辑。
- 初步复制用户代码页、用户数据页和用户栈页，避免父子直接共享同一用户栈物理页。
- 新增阻塞式用户态 `waitpid` 最小语义：父进程等待时切换到子进程运行，子进程 exit 后恢复父进程。
- 新增 `fork` 测试程序，验证 fork、exit、waitpid 的闭环。

修改文件：

- `boot/interrupt.asm`
- `include/elf.h`
- `include/process.h`
- `include/syscall.h`
- `kernel/elf.c`
- `kernel/fs.c`
- `kernel/process.c`
- `kernel/syscall.c`
- `readme.md`
- `docs/task25_process.md`
- `docs/task30_fork.md`

验证结果（最小场景）：

- `run hello` 仍可正常运行、退出，并由 shell `waitpid` 回收。
- `run fork` 可输出 `parent after fork`、`child after fork`、`parent wait done`。
- fork 子进程退出后，父进程可从用户态 `waitpid(child_pid)` 返回。
- 连续两次执行 `run fork` 均可正常完成，不出现明显 page fault 或重复释放。

当前能力提升：

- MiniOS 已具备最小教学版 fork 闭环：fork -> child exit -> parent waitpid。
- 在尚未实现独立页目录和 COW 前，已经把“复制进程”和“重新 exec 程序”从语义上区分开。

TODO：

- 暂不支持 copy-on-write。
- 暂不支持文件描述符复制。
- 暂不支持复杂 VMA。
- 暂不支持通用时间片调度下的多进程压力测试。
- 当前仍采用共享内核页表 + 按进程重装用户页的教学版模型，不是完整多地址空间实现。

## ✅ Task31：fork 后的调度与父子执行顺序验证

本轮目标：

- 通过最小用户态测试程序和轻量级内核调试输出，验证 fork 后父子进程的返回值、继续执行位置、waitpid 回收路径和用户栈独立性。

已完成：

- 调整 `fork` 测试程序，新增 `before fork`、`parent: child pid`、`child: fork returned 0`、`parent: waitpid done` 输出。
- 新增轻量级 fork 调试日志，输出父 pid、子 pid、`parent_pid`、用户态 `eip/esp`、父子用户栈物理页地址。
- 验证父进程 fork 返回 `child_pid`，子进程 fork 返回 `0`。
- 验证父子都从 fork 返回点之后继续执行，且子进程不会从程序入口重新开始。
- 验证 waitpid 可以回收正确的 fork 子进程。
- 初步验证父子用户栈虚拟地址相同但物理页不同。

修改文件：

- `kernel/process.c`
- `kernel/syscall.c`
- `kernel/fs.c`
- `readme.md`
- `docs/task25_process.md`
- `docs/task31_fork_validation.md`

验证结果（最小场景）：

- `run fork` 输出中 `before fork` 只出现一次，说明子进程不是从程序入口重启。
- `run fork` 可稳定看到父分支、子分支和 `waitpid` 完成提示。
- 连续两次执行 `run fork` 仍可正常完成，不出现明显 page fault 或 PCB 泄漏。
- `run hello` + shell `waitpid 1` 仍可正常回收，原有 exec/exit/waitpid 流程未被破坏。

TODO：

- 当前仍未覆盖复杂多子进程竞争回收场景。
- 当前仍未覆盖 fork 后更复杂的用户栈数据读写自检。
- 当前仍未实现真正的独立页目录与通用调度器验证。
