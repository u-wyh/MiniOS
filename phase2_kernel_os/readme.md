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

### Task35：用户态输入路径雏形

已完成：

- 在键盘中断处理路径中加入最小内核输入缓冲区。
- 新增 `read_char` syscall，用户态可逐字符读取键盘输入。
- 用户态 shell 可以在固定脚本执行后等待一个按键，并输出读取结果。
- 初步打通 `keyboard IRQ -> kernel buffer -> syscall -> user shell` 的最小输入链路。

当前最小语义：

- 键盘 IRQ1 在拿到可打印字符后，会先把字符写入内核环形缓冲区。
- `SYS_READ_CHAR` 有字符时返回一个字符；无字符时先在内核里休眠等待后续中断，再继续检查输入缓冲区。
- 这样用户态 shell 仍然按“逐字符读取”的方式工作，但不会因为空转轮询把 CPU 长时间打满。

TODO：

- 暂不支持完整行编辑。
- 暂不支持方向键和复杂特殊键。
- 暂不支持阻塞等待队列。
- 暂不支持完整 TTY / stdin 抽象。

### Task36：最小交互式 Shell 命令解析

已完成：

- 用户态 shell 可以通过 `read_char` syscall 轮询读取一整行输入。
- shell 在用户态维护固定长度行缓冲区，并以 `'\0'` 结尾形成最小命令字符串。
- shell 支持 `help` / `hello` / `exit` 三个固定命令。
- `hello` 命令通过 `fork -> child exec(hello) -> parent waitpid` 路径执行固定用户程序。
- `exit` 命令会让 shell 退出，并由 `init` 执行 `waitpid(shell_pid)` 回收。
- 初步形成 `init -> shell -> user program` 的最小交互式用户态闭环。

TODO：

- 暂不支持参数解析。
- 暂不支持 PATH 搜索。
- 暂不支持 `argv/envp`。
- 暂不支持管道和重定向。
- 暂不支持完整行编辑和历史记录。

### Task37：用户态 Shell 参数解析雏形

已完成：

- shell 可以将输入行按空格拆分为有限数量参数。
- 新增 `echo <text>` 内建命令。
- 新增 `run <program>` 命令。
- `run hello` 通过 `fork / exec / waitpid` 启动固定内置程序。
- `help` 输出更新为当前支持的命令列表，并保留 `hello` 作为 `run hello` 的快捷方式。

TODO：

- 暂不支持引号和转义。
- 暂不支持 `argv/envp` 传递给被执行程序。
- 暂不支持 PATH 搜索。
- 暂不支持管道和重定向。
- 暂不支持真实文件系统加载 ELF。

### Task38：用户态程序 argv 传递雏形

已完成：

- shell 的 `run` 命令可以向被执行的固定用户程序传递少量参数。
- 在 PCB 中新增教学版 `argc/argv` 暂存区，并通过最小 syscall 暴露给用户程序读取。
- 新增用户态 `echo` 程序。
- `run echo hello` / `run echo hello minios` 可通过 `fork / exec / waitpid` 执行并输出参数。
- `run hello` 保持兼容，原有 `init -> shell -> user program` 路径未被破坏。

TODO：

- 当前 `argv` 采用 PCB 暂存区，不是真实用户栈 ABI。
- 暂不支持 `envp`。
- 暂不支持引号和转义。
- 暂不支持 PATH 搜索。
- 暂不支持真实文件系统 exec。

### Task41：前台 / 后台任务雏形

已完成：

- 明确 `run <program>` 为前台执行路径：shell 在父进程中 `waitpid` 子进程。
- 提供 `start <program>` 后台启动语义：shell 在 `fork/exec` 后不等待，立即返回提示符。
- 提供 `wait <pid>` 手动回收入口：用于回收后台子进程（含被 kill 后的 zombie）。
- `ps / kill / wait` 可以组合使用，用于观察、终止和回收后台任务。
- 保持 `init -> shell -> user program` 父子链路不变，避免破坏既有 `run/exit` 流程。

TODO：

- 暂不支持 `&` 语法。
- 暂不支持 `jobs`。
- 暂不支持 `fg/bg`。
- 暂不支持进程组与终端控制。
- shell 退出后的 orphan/reparent 机制仍是后续增强点。

### Task42：孤儿进程 reparent 到 init 雏形

已完成：

- 在进程模块中记录教学版 `init_pid`。
- 父进程退出时扫描进程表，将其仍有效的子进程 `parent_pid` 迁移到 `init_pid`。
- reparent 只改变父子关系，不改变子进程运行状态，不直接释放子进程资源。
- shell 退出后，其后台子进程不会失去父进程，可被 init 接管。

TODO：

- init 暂未实现完整自动 wait/reaper 循环。
- 暂不支持 `wait -1`。
- 暂不支持进程组、session 与终端控制。
- 暂不支持完整 job control（`jobs/fg/bg`）。

### Task43：init reaper 循环雏形

已完成：

- 新增 `wait_any`（`SYS_WAIT_ANY`）非阻塞回收接口。
- `init` 在回收 shell 后进入最小 reaper 循环，周期性尝试回收自己的 ZOMBIE 子进程。
- 被 reparent 到 init 的孤儿进程退出后，可被 init 的 `wait_any` 回收。
- 打通 `reparent -> zombie -> init reap` 的最小闭环。

TODO：

- `wait_any` 暂不支持阻塞等待语义。
- 暂不支持 `SIGCHLD`。
- 暂不支持完整 `wait(-1)` 语义与 wait 队列。
- 暂不支持复杂 init 服务管理与完整 job control。

### Task44：用户态 yield / sleep 系统调用雏形

已完成：

- 新增 `SYS_YIELD`，用户态进程可主动让出 CPU。
- 新增 `SYS_SLEEP`，用户态进程可按 tick 粒度进入 `SLEEPING`。
- PCB 增加 `wakeup_tick` 字段，用于记录睡眠到期时间。
- PIT 每次 tick 会扫描并唤醒到期的 `SLEEPING` 进程（改回 `READY`）。
- `ps` 状态名补充 `SLEEPING`，便于观察睡眠状态。

TODO：

- `sleep` 目前是 tick 粒度，不是高精度定时器。
- 暂无复杂阻塞队列与信号唤醒。
- 暂无 `select/poll` 一类复用接口。
- 用户态内置测试程序对 `sleep/yield` 的覆盖还可继续增强。

### Task45：用户态 uptime / ticks 命令雏形

已完成：

- 复用 PIT 现有 tick 计数，新增只读 `SYS_GET_TICKS` 查询接口。
- 用户态 shell 新增 `uptime` 命令，并支持 `ticks` 作为别名。
- `uptime` 可显示系统启动以来累计的 PIT tick 数。
- 连续执行 `uptime` 时可以观察 tick 持续递增。
- 为后续 sleep 命令增强、进程运行时间统计和调度统计打基础。

TODO：

- 当前 `uptime` 只显示 tick 数，不显示秒或真实日期时间。
- 暂不支持 RTC 真实时间。
- 暂不处理 tick 溢出。
- 暂不统计每个进程的 CPU 时间。

### Task46：用户态 sleep 命令雏形

已完成：

- 用户态 shell 新增 `sleep <ticks>` 命令，直接复用已有 `SYS_SLEEP`。
- `sleep` 缺少参数时会输出 `Usage: sleep <ticks>`。
- `sleep` 参数非法时会输出 `Invalid ticks`。
- `sleep 0` 当前直接返回，不额外进入 sleep/yield，保持最小语义简单稳定。
- `sleep <ticks>` 会让当前 shell 进入 `SLEEPING`，到期后再恢复并重新显示提示符。
- 当系统里还有其他活动进程时，`sleep <ticks>` 优先复用已有 `SYS_SLEEP`；当前仅剩 `init + shell` 时，会回退到基于 `get_ticks` 的最小等待，保证命令语义稳定。
- 兼容保留了原有教学调试语义：`sleep <pid> <ticks>` 仍可按 pid 让目标进程睡眠。
- 可通过 `uptime -> sleep -> uptime` 观察 tick 差值增长，验证 sleep 生效。

TODO：

- `sleep` 单位暂为 tick，不是秒。
- 暂不支持 `sleep 1s` / `sleep 1m` 等格式。
- 暂不支持高精度计时。
- 暂不支持 signal 中断 sleep。
- 暂不支持复杂阻塞队列。

### Task47：进程创建时间 / 存活时间统计雏形

已完成：

- PCB 新增 `create_tick`，用于记录进程创建时的系统 tick。
- 新创建进程会复用已有 `pit_get_ticks()` 写入 `create_tick`，不新增第二套 tick 计数。
- `fork` 子进程会重新记录自己的 `create_tick`；`exec` 只替换镜像，不重置同一 pid 的创建时间。
- PCB 回收回 `UNUSED` 时会统一清理 `create_tick`，避免后续复用槽位时出现脏数据。
- `process_info` 新增 `age_ticks` 字段，`ps` 查询时按 `当前 tick - create_tick` 计算。
- shell `ps` 输出新增 `AGE` 列，可观察 `init / shell / loop / sleep_test` 等进程已存在多久。
- 新增最小 `sleep_test` 用户程序，便于观察 `SLEEPING` 状态和 AGE 增长。
- 为后续 CPU 运行 tick、调度次数等统计保留了基础字段，但本轮不实现这些复杂指标。

TODO：

- `AGE` 当前表示进程存活时间，不是 CPU 使用时间。
- 暂不统计用户态 / 内核态运行时间。
- 暂不统计上下文切换次数。
- 暂不处理 tick 溢出。
- 暂无 `top` 命令或复杂性能监控。

### Task48：进程运行 tick / 调度次数统计雏形

已完成：

- PCB 新增 `schedule_count` 字段，用于记录进程被调度器选中运行的次数。
- 新进程创建时 `schedule_count` 初始化为 `0`，PCB 回收时会统一清零。
- 在真正决定“下一个由谁运行”的位置递增调度次数，而不是在遍历候选进程时递增。
- `process_info` 新增 `runs` 字段，用户态 `ps` 可直接显示调度次数。
- shell `ps` 输出新增 `RUNS` 列。
- `AGE` 表示存活时间，`RUNS` 表示被调度次数，两者语义保持分离。
- 为后续 CPU 时间统计和更丰富的调度观测打下基础，但本轮不实现 CPU 占用率。

TODO：

- `RUNS` 不是 CPU 占用率。
- 暂不统计实际运行 tick。
- 暂不区分用户态 / 内核态运行时间。
- 暂不统计上下文切换耗时。
- 暂无 `top` 命令。

### Task49：系统调用表整理与文档化

已完成：

- 重新整理 `syscall.h` 中的 syscall 编号分组，保持现有编号不变，但把命名、用途和边界说明写清楚。
- 补充 `syscall.c` 分发注释，明确当前 ABI 中 syscall 编号、参数寄存器和返回值寄存器约定。
- 整理当前用户态封装函数和对应 syscall 的关系，明确 shell 主要依赖的最小封装接口。
- 新增 `docs/syscall.md`，集中记录 MiniOS Phase2 当前 syscall 总表、参数位置、返回值语义和限制。
- 明确当前 MiniOS syscall ABI 仍是教学版最小接口，不引入 `errno`、文件描述符表或复杂权限模型。

TODO：

- 暂无 `errno`。
- 暂无完整用户指针校验。
- 暂无完整文件描述符表。
- `exec` 仍基于内置 `program_id`。
- syscall ABI 后续 Phase3 可能继续调整。

### Task50：用户程序表 / program_id 整理

已完成：

- 新增统一的 `program_id` 定义，集中管理内置用户程序编号，避免 shell / process / exec 路径继续散落魔法数字。
- 新增统一用户程序描述表，集中维护 `program_id -> program name` 的规范映射。
- `process_exec_program_args` 现在直接通过统一程序表解析 `program_id -> ELF/blob`，不再自己维护另一套固定编号映射。
- shell `run/start` 现在通过共享程序清单解析 `program name -> program_id`，与内核侧使用同一份程序定义。
- 当前内置用户程序表已整理为：`init`、`shell`、`hello`、`echo`、`loop`、`loop_exit`、`sleep_test`，并保留 `execchild` / `info` / `fork` / `forkexec` 等历史教学程序条目。
- 新增最小 `loop_exit` 用户程序，便于验证“会主动退出的循环程序”路径。

TODO：

- 当前仍不支持真实文件系统。
- 当前仍不支持 `PATH` 搜索。
- 当前仍不支持动态加载外部 ELF 文件。
- 当前仍不支持 `envp` 等更完整的进程启动环境。

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

## ✅ Task32：fork + exec + waitpid 最小组合路径

本轮目标：

- 验证父进程 fork 出子进程后，子进程可以 exec 到另一个固定内置用户程序，随后由父进程 waitpid 回收。

已完成：

- 新增最小 `SYS_EXEC(program_id)`，复用现有 `process_exec_file` 完成用户镜像替换。
- 新增 `execchild` 测试程序，作为子进程 exec 后的新程序入口。
- 新增 `forkexec` 测试程序，验证 `fork -> child exec -> child exit -> parent waitpid` 闭环。
- 验证 exec 前后子进程 `pid` 保持不变，`parent_pid` 保持不变。
- 验证 exec 成功后直接进入新程序入口，不继续执行旧程序中的 fallback `exit(99)`。
- 验证新程序 `exit(7)` 后，父进程 `waitpid(child_pid)` 能正确回收子进程。

修改文件：

- `include/process.h`
- `include/syscall.h`
- `kernel/process.c`
- `kernel/syscall.c`
- `kernel/fs.c`
- `readme.md`
- `docs/task25_process.md`
- `docs/task32_fork_exec_waitpid.md`

验证结果（最小场景）：

- `run forkexec` 可看到 `child: call exec` 后进入 `exec child running`。
- exec 前后子进程 `pid` 与 `parent_pid` 保持不变。
- `parent: waitpid done` 可稳定输出，说明 waitpid 回收成功。
- 连续两次执行 `run forkexec` 不崩溃。
- 原有 `run fork` 与 `run hello` 路径仍可正常工作。

TODO：

- 暂不支持 `argv/envp`。
- 暂不支持路径字符串与 PATH 搜索。
- 暂不支持文件描述符继承。
- 暂不支持真实文件系统中的可执行文件。
- 暂不支持更完整的多进程调度压力测试。

## ✅ Task33：最小用户态 init 进程

本轮目标：

- 在内核完成基础初始化后，自动启动第一个固定用户态 `init` 进程。
- 由 `init` 在用户态执行最小 `fork + exec + waitpid` 逻辑，逐步替代内核直接驱动这条测试路径。

已完成：

- 在 `kernel_main` 完成分页、中断、PIT 和进程表初始化后，直接启动固定用户态 `init` 程序。
- `init` 成为当前系统里第一个用户进程，因此默认获得 `pid 1`。
- `init` 在用户态输出 `init start`，随后执行 `fork`。
- `init` 的子进程 `exec` 到固定内置程序 `execchild`，并保持原有 `pid / parent_pid` 不变。
- `execchild` 输出 `exec child running` 后执行 `exit(7)`。
- `init` 通过 `waitpid(child_pid)` 回收子进程，并输出 `init wait done`。
- `init` 完成测试后执行 `exit(0)`，系统稳定返回原有内核 shell，保留后续调试入口。

修改文件：

- `kernel/kernel.c`
- `kernel/fs.c`
- `readme.md`
- `docs/task25_process.md`
- `docs/task33_init_process.md`

验证结果（最小场景）：

- 开机后可直接看到：
  - `kernel: start init`
  - `init start`
  - `init child: call exec`
  - `exec child running`
  - `init wait done`
- fork 调试日志显示：
  - `init` 父进程 `pid = 1`
  - 子进程 `pid = 2`
  - 子进程 `parent_pid = 1`
- exec 调试日志显示子进程在替换前后保持相同 `pid = 2` 与 `parent_pid = 1`。
- `init` 退出后系统稳定回到 `MiniOS>` shell 提示符。
- 回归验证：
  - `run forkexec` 仍可正常跑通。
  - `run hello` 后 shell `waitpid` 仍可正常回收。

TODO：

- 暂不支持用户态 shell。
- 暂不支持命令解析。
- 暂不支持 `argv/envp`。
- 暂不支持路径字符串 `exec`。
- 暂不支持真实文件系统加载 ELF。

## ✅ Task34：最小用户态 Shell 雏形

本轮目标：

- 在 `init` 之下新增一个最小用户态 `shell` 程序。
- `shell` 暂时不是交互式，而是固定脚本式：启动后打印标记，自动执行一个固定命令，再退出。

已完成：

- 新增固定内置 `shell` 用户程序。
- `init` 不再直接 `exec` 普通测试子程序，而是先 `fork` 出子进程并 `exec` 到 `shell`。
- `shell` 在用户态输出 `shell start` 和 `MiniOS$ run hello`。
- `shell` 自己再执行一轮 `fork -> exec -> waitpid`，把子进程替换成固定的 `hello` 用户程序。
- `hello` 运行后输出 `Hello from user ELF` 并 `exit`。
- `shell` 通过 `waitpid` 回收 `hello` 子进程，输出 `MiniOS$ done`，随后退出。
- `init` 再通过 `waitpid(shell_pid)` 回收 `shell`，输出 `init shell exited`。

修改文件：

- `kernel/process.c`
- `kernel/fs.c`
- `readme.md`
- `docs/task25_process.md`
- `docs/task34_user_shell.md`

验证结果（最小场景）：

- 开机自动路径中可观察到：
  - `kernel: start init`
  - `init child: call shell`
  - `shell start`
  - `MiniOS$ run hello`
  - `Hello from user ELF`
  - `MiniOS$ done`
  - `init shell exited`
- 父子关系调试日志显示：
  - `init -> shell`：`[fork] clone a=1 b=2 c=1`
  - `shell -> hello`：`[fork] clone a=2 b=3 c=2`
- `shell` 的子进程 `exec` 到 `hello` 时，日志显示 `pid = 3`、`parent_pid = 2` 保持不变。
- 回归验证：
  - `run forkexec` 仍可正常执行。
  - 原有 `fork/exec/waitpid` 单独路径没有被破坏。

TODO：

- 暂不支持交互式 shell。
- 暂不支持键盘输入接入用户态 shell。
- 暂不支持命令解析。
- 暂不支持 `argv/envp`。
- 暂不支持 PATH 搜索。
- 暂不支持真实文件系统加载 ELF。

## ✅ Task36：最小交互式 Shell 命令解析

本轮目标：

- 让用户态 `shell` 从固定脚本式流程升级为最小交互式流程。
- 保持 `fork/exec/waitpid` 主路径不变，只增加最小读行和固定命令分发。

已完成：

- `shell` 启动后打印 `shell start`，随后进入交互循环并显示 `MiniOS$ ` 提示符。
- `shell` 通过 `read_char` syscall 在用户态忙等读取字符，并在本地固定长度缓冲区中拼出一行命令。
- 当前支持三个固定命令：`help`、`hello`、`exit`。
- `help` 由 `shell` 自己处理，输出可用命令列表。
- `hello` 由 `shell` 先 `fork`，子进程再 `exec` 到固定内置 `hello` 程序，父进程 `waitpid` 回收。
- `exit` 会让 `shell` 退出；父进程 `init` 通过 `waitpid(shell_pid)` 回收它，并输出 `init shell exited`。
- 未知命令会输出 `Unknown command`，空输入则直接重新显示提示符。

修改文件：

- `kernel/fs.c`
- `readme.md`
- `docs/task7_input.md`
- `docs/task25_process.md`
- `docs/task36_interactive_shell.md`

验证结果（最小场景）：

- 启动后可看到：
  - `kernel: start init`
  - `init start`
  - `shell start`
  - `MiniOS$ `
- 输入 `help` 后会输出 `commands: help hello exit`，随后返回提示符。
- 输入 `hello` 后会输出 `Hello from user ELF`，并回到提示符。
- 输入未知命令如 `abc` 后会输出 `Unknown command`，系统保持稳定。
- 直接回车时不会崩溃，而是重新显示提示符。
- 连续执行 `help`、`hello`、`hello`、`exit` 后，`init` 能正常回收 `shell`，未观察到 page fault 或重复释放。

TODO：

- 暂不支持参数解析。
- 暂不支持 PATH 搜索。
- 暂不支持 `argv/envp`。
- 暂不支持管道、重定向和文件描述符表。
- 暂不支持完整 readline、方向键和历史记录。

## ✅ Task37：用户态 Shell 参数解析雏形

本轮目标：

- 在已有用户态交互式 shell 上增加最小参数解析能力。
- 保持 `fork/exec/waitpid` 主路径不变，只增加 token 拆分、`echo` 内建命令和 `run <program>` 分发。

已完成：

- shell 会先读取一整行命令，再按空格拆成有限数量 token。
- 当前支持命令：`help`、`echo <text>`、`run hello`、`hello`、`exit`。
- `echo` 作为 shell 内建命令在用户态直接拼接并输出参数，不需要 `fork`。
- `run <program>` 当前采用固定内置程序映射，已支持 `run hello`。
- `hello` 被保留为 `run hello` 的快捷方式，兼容上一轮最小交互测试。
- `run` 缺少参数时会输出 `Usage: run <program>`。
- `run` 目标未知时会输出 `Unknown program`；未知命令仍输出 `Unknown command`。

修改文件：

- `kernel/fs.c`
- `readme.md`
- `docs/task7_input.md`
- `docs/task25_process.md`
- `docs/task37_shell_args.md`

验证结果（最小场景）：

- 启动后仍可看到 `shell start` 和 `MiniOS$ ` 提示符。
- `help` 会输出当前支持的命令列表。
- `echo hello` 会输出 `hello`。
- `echo hello minios` 会输出 `hello minios`。
- `run hello` 会输出 `Hello from user ELF`，随后回到提示符。
- `run abc` 会输出 `Unknown program`。
- `abc` 会输出 `Unknown command`。
- 空输入不会崩溃；`echo    hello` 和 `run     hello` 也能正确跳过多余空格。
- `exit` 后 `init` 仍能回收 `shell`。

TODO：

- 暂不支持引号和转义。
- 暂不支持 `argv/envp` 传递给被执行程序。
- 暂不支持 PATH 搜索。
- 暂不支持管道、重定向和文件描述符表。
- 暂不支持真实文件系统加载 ELF。

## ✅ Task38：用户态程序 argv 传递雏形

本轮目标：

- 在当前最小 `run <program>` 的基础上，让 shell 可以把少量参数传给被执行的用户程序。
- 保持 `fork / exec / waitpid` 执行链不变，只补上教学版 `argc/argv` 传递能力。

已完成：

- 在 PCB 中新增教学版 `user_argc + user_argv[][]` 暂存区。
- 新增 `SYS_GET_ARGC` / `SYS_GET_ARG`，供用户程序读取自己的启动参数。
- 新增 `SYS_EXEC_ARGS`，让 shell 可以把 `run` 后面的少量参数传给目标程序。
- 新增用户态 `echo` 程序，能够读取自己的 `argc/argv` 并输出 `argv[1..argc-1]`。
- shell 现在支持 `run echo`、`run echo hello`、`run echo hello minios`。
- `run hello` 仍保持兼容，继续通过 `fork / exec / waitpid` 运行。
- 参数过多或参数过长时，shell 会给出简单错误提示，不会导致内核崩溃。

修改文件：

- `include/process.h`
- `include/syscall.h`
- `kernel/process.c`
- `kernel/syscall.c`
- `kernel/fs.c`
- `readme.md`
- `docs/task7_input.md`
- `docs/task25_process.md`
- `docs/task38_argv.md`

验证结果（最小场景）：

- `run hello`
- `run echo hello`
- `run echo hello minios`
- `run echo`
- `run abc`
- 参数过多
- 参数过长
- `exit`

当前限制：

- 当前 `argv` 暂存在 PCB 中，不是真实用户栈上的 `argc/argv` ABI。
- 暂不支持 `envp`。
- 暂不支持引号和转义。
- 暂不支持 PATH 搜索。
- 暂不支持真实文件系统加载 ELF。

## ✅ Task39：用户态 ps 命令雏形

本轮目标：

- 为用户态 shell 增加最小 `ps` 命令。
- 通过 syscall 读取内核进程表摘要，并在用户态打印 `pid/ppid/state/name`。

已完成：

- 新增 `SYS_PS` 系统调用（最小语义：按活动进程序号读取一条 `process_info` 摘要）。
- 新增用户可见结构 `process_info`，字段包含：`pid`、`ppid`、`state`、`name`。
- 内核新增只读导出函数，遍历活动进程并返回摘要，不暴露 PCB 指针。
- 用户态 shell 新增 `ps` 内建命令，会打印表头和每条进程记录。
- `help` 已同步加入 `ps`。
- 兼容验证通过：`run hello`、`run echo <args>`、`exit` 路径保持可用。

当前限制：

- `ps` 暂不支持参数。
- 暂不支持进程树显示。
- 暂不支持 CPU 时间、内存占用等统计。
- 暂无 `/proc` 文件系统。

## ✅ Task40：用户态 kill 命令雏形

本轮目标：

- 在用户态 shell 增加 `kill <pid>`，通过最小 syscall 请求内核终止普通用户进程。
- 保持当前 `fork/exec/waitpid/ps` 主路径稳定，不引入完整 signal 子系统。

已完成：

- 新增 `SYS_KILL`，内核调用 `process_kill(pid, -9)` 执行教学版终止。
- 新增 `process_kill`：
  - 支持按 pid 查找并将目标置为 `ZOMBIE`。
  - 设置退出码为固定值（`-9`）。
  - 不在 kill 时直接释放资源，保留给后续 `waitpid` 回收。
  - 最小保护：拒绝杀 `init(pid=1)` 和当前进程（当前 shell）。
- shell 新增 `kill <pid>` 命令：
  - 参数缺失输出 `Usage: kill <pid>`
  - 非法 pid 输出 `Invalid pid`
  - 成功输出 `Killed`
  - 失败输出 `Kill failed`
- 为便于验证 kill 新增最小辅助命令：
  - `start <program> [args...]`：只 `fork/exec`，不 `waitpid`
  - `wait <pid>`：最小包装 `waitpid(pid)`
- 程序映射新增 `loop`（可通过 `start loop` 启动后再 kill 观察）。
- `help` 已同步包含 `ps`、`kill`、`start`、`wait`。

当前限制：

- 当前 kill 不是 Linux signal 系统。
- 不支持 `SIGKILL/SIGTERM` 编号、handler、进程组、权限模型。
- 不支持 `killall` 和完整后台任务管理。
- kill 后 zombie 的完整资源释放仍依赖父进程 `waitpid`。

## ✅ Task51：用户程序参数传递整理 / argc argv 语义统一

本轮目标：

- 整理 shell `run/start` 到 `exec` 的用户程序参数传递链路。
- 明确 `program name`、`argc`、`argv` 的当前最小语义。
- 统一参数数量与长度边界，避免静默截断或越界复制。

已完成：

- 统一参数上限常量：当前最多支持 `8` 个程序参数（含 `argv[0]` 程序名）。
- 统一单参数长度上限：当前每个参数最多 `31` 个可见字符，另保留 `'\0'` 结尾。
- shell `run/start` 现在明确采用：
  - `argv[0] = program name`
  - 剩余 token 作为用户参数
- shell 在 `fork/exec` 前会先检查：
  - 程序参数是否过多
  - 单个参数是否过长
- 参数过多时直接输出 `Too many args`，参数过长时输出 `Arg too long`。
- 内核 `process_copy_user_args()` 继续保留兜底校验，避免非法参数破坏 PCB。
- `echo` 继续作为最直接的参数传递验证程序：
  - `run echo`
  - `run echo hello`
  - `run echo hello minios phase2`

当前语义：

- `run echo hello minios`
- shell 自己看到的是：`run` / `echo` / `hello` / `minios`
- 被执行的用户程序 `echo` 看到的是：
  - `argv[0] = "echo"`
  - `argv[1] = "hello"`
  - `argv[2] = "minios"`

当前限制：

- 当前 `argv` 仍保存在 PCB 暂存区，不是真实用户栈 `argc/argv` ABI。
- 暂不支持 `envp`。
- 暂不支持 `PATH`。
- 暂不支持复杂引号、转义、管道和重定向。
- 暂不支持从磁盘加载外部程序。

## ✅ Task52：用户程序退出状态 / wait 语义整理

本轮目标：

- 整理用户程序 `exit(status)`、`wait`、`reaper` 和 `ps` 状态显示之间的生命周期闭环。
- 让前台 `run`、后台 `start`、手动 `wait` 的关系更加清楚。

已完成：

- 明确保留教学版 `ZOMBIE` 语义：用户程序退出后先保留 `pid` 与 `exit_status`，等待父进程或 init/reaper 回收。
- `exit(status)` 后，退出进程不再继续参与调度。
- shell 现在支持 `wait [pid]`：
  - `wait`：非阻塞回收任意一个已退出子进程
  - `wait <pid>`：对指定子进程走当前最小 `waitpid` 语义
- `ps` 增加 `EXIT` 列，用于观察当前进程记录的 `exit_status`。
- `run loop_exit`、`start loop_exit`、`wait`、`ps` 可以组成一条较完整的退出/回收验证链路。

当前限制：

- 当前仍不是完整 Linux `waitpid`
- 暂不支持信号系统
- 暂不支持进程组、session 和 TTY 控制
- `exit_status` 目前主要通过 `ps` 观察，`wait` 返回值仍以 `pid` 为主

## ✅ Task53：进程父子关系 / reparent 语义整理

本轮目标：

- 明确 `parent_pid` 的教学版语义。
- 让 init、shell、用户程序之间的父子关系可通过 `ps` 观察。
- 父进程退出或被 kill 时，把仍存在的子进程转交给 init。
- 让 wait/reaper 与父子关系配合，避免误回收无关进程。

已完成：

- 约定 `PROCESS_ROOT_PARENT_PID = 0`，表示 init 没有普通父进程。
- 新进程默认以当前运行进程作为父进程，因此：
  - init 的 PPID 为 `0`
  - shell 的 PPID 指向 init
  - shell 通过 `run/start` 创建的用户程序 PPID 指向 shell
- `wait` / `waitpid` / `wait_any` 继续只回收当前进程名下的子进程。
- 父进程 `exit` 或被 `kill` 时，会把仍有效的子进程 reparent 给 init。
- init/reaper 会兜底回收已经挂到 init 名下、且没有父进程正在等待的孤儿 zombie。
- `ps` 中的 `PPID` 可用于观察当前父子关系。

当前限制：

- 不实现完整 Linux `waitpid`
- 不新增信号系统
- 不支持进程组、session 和 TTY 控制
- 当前 reparent 只维护 `parent_pid`，不引入复杂进程树结构

## ✅ Task54：kill syscall / shell kill 命令整理

本轮目标：

- 整理 `SYS_KILL` 与 shell `kill <pid>` 的教学版进程终止语义。
- 让后台 `loop` 这类长期运行程序可以被 pid 控制。
- 明确 kill 后仍复用 `ZOMBIE -> wait/reaper -> free slot` 生命周期。

已完成：

- `SYS_KILL(pid)` 复用现有 syscall ABI：`ebx=pid`，成功返回 `0`，失败返回负值。
- `process_kill()` 通过 pid 查找目标进程，并把可终止目标标记为 `PROCESS_ZOMBIE`。
- kill 后写入 `PROCESS_KILL_EXIT_STATUS`，该值只表示“被 kill”，不是 Unix/Linux 信号编号。
- 被 kill 的进程不再处于 READY/RUNNING/SLEEPING 调度候选状态。
- 被 kill 的进程仍由父进程 `wait/waitpid/wait_any` 或 init/reaper 回收。
- shell `kill <pid>` 支持：
  - 缺少 pid 时输出 `Usage: kill <pid>`
  - 非数字 pid 时输出 `Invalid pid`
  - 成功时输出 `Killed`
  - 失败时输出 `Kill failed`

当前限制：

- 不实现完整信号系统
- 不支持 `kill -9` 或其它信号编号
- 不支持进程组 kill
- 不支持权限模型
- 不允许 kill init，也不允许当前 shell 直接 kill 自己

## ✅ Task55：Shell 前后台任务观察 / jobs 命令整理

本轮目标：

- 增加 shell `jobs` 命令，用当前 shell 的后台任务视角观察进程。
- 区分 `ps` 的全局进程表视角和 `jobs` 的 shell 后台子进程视角。
- 让 `start loop` 后可以用 `jobs` 直接看到后台任务 pid、状态和程序名。

已完成：

- `process_info` 摘要新增 `is_background`，供用户态 shell 判断后台任务。
- shell 新增 `jobs` 命令：
  - 无后台任务时输出 `No background jobs`
  - 有后台任务时显示 `JOB / PID / STATE / NAME`
- `jobs` 只显示当前 shell 直接管理的后台子进程，不显示 init、shell 自身和前台 `run` 程序。
- `jobs` 只做观察，不负责释放资源；退出/被 kill 的后台任务仍由 `wait` 或 init/reaper 回收。

当前限制：

- 不实现 `fg`
- 不实现 `bg`
- 不实现 Ctrl+Z
- 不支持 `SIGSTOP/SIGCONT`
- 不支持进程组、session 和 tty 前台控制
- 当前 `JOB` 列只是按遍历顺序生成的教学版显示编号，不是持久 job id

## ✅ Task56：系统 tick / sleep / uptime 语义整理

本轮目标：

- 把 `PIT IRQ0 -> ticks -> sleep / wakeup -> uptime` 这条时间链路整理清楚。
- 明确当前 MiniOS 内部统一使用 tick 作为时间单位。
- 让 `uptime` 和 `sleep_test` 能更直观地验证时间推进。

已完成：

- 统一复用 `pit_get_ticks()` 作为当前系统 tick 读取接口。
- 新增 `pit_get_frequency()`，明确当前 PIT 默认频率为 `20Hz`。
- shell `uptime` / `ticks` 现在显示：
  - `ticks: <n>`
  - `seconds: <n / 20>`
- `sleep(ticks)` 语义保持为“睡眠若干个 tick”，`sleep(0)` 继续退化为 `yield`。
- `sleep_test` 现在会输出 sleep 前后的 tick，便于观察 wakeup 是否按预期发生。
- `SLEEPING` 进程仍只会在 `wakeup_tick` 到期后恢复为 `READY`，不会在睡眠期间持续占用 CPU。

当前限制：

- 不实现 RTC 真实日期时间
- 不支持时区和 wall clock
- 不支持高精度定时器
- 不支持 `nanosleep`
- 不支持 `timerfd`
- 不支持信号唤醒

## ✅ Task57：内核内置只读文件表 / ls、cat 雏形

本轮目标：

- 引入教学版只读文件抽象，但不进入真实磁盘文件系统。
- 在内核中维护统一的内置只读文件表。
- 让 shell 先支持最小 `ls` / `cat <file>`。

已完成：

- 在 `fs.h` 中统一维护内置只读文本文件清单。
- 当前内置文件包括：
  - `/readme.txt`
  - `/programs`
  - `/help.txt`
- `fs.c` 新增只读文本文件查询接口：
  - `fs_builtin_file_count()`
  - `fs_builtin_file_at()`
  - `fs_builtin_file_find()`
- 用户态 shell 新增 `ls` 命令，可列出内置文件名和大小。
- 用户态 shell 新增 `cat <file>` 命令，可输出内置文件内容。
- `cat` 缺少参数和文件不存在时都会给出清晰错误。

当前限制：

- 当前文件来自内核静态只读数据，不来自真实磁盘。
- 不实现真实块设备、ext2/FAT、目录树、权限、inode 和持久化写入。
- 本轮不实现 `open/read/close` syscall。
- 后续可在这张只读文件表基础上继续扩展 fd 表与 `read` syscall。

## ✅ Task58：只读文件描述符 / open-read-close syscall 雏形

本轮目标：

- 从 Task57 的内置只读文件表继续推进到教学版 fd 抽象。
- 增加最小 `open/read/close` syscall。
- 让 `cat <file>` 优先通过 fd 层读取文件内容。

已完成：

- 在 PCB 中新增每进程 fd 表，表项记录：
  - `used`
  - `file`
  - `offset`
- 当前 fd 从 `3` 开始分配，`0/1/2` 仅保留语义，不在本轮完整实现。
- 新增：
  - `SYS_OPEN`
  - `SYS_READ`
  - `SYS_CLOSE`
- `read(fd, buf, size)` 支持：
  - offset 前进
  - EOF 返回 `0`
  - 非法 fd / 已关闭 fd 安全失败
- `close(fd)` 后 fd 失效，可被重新分配。
- shell `cat <file>` 现在优先通过 `open -> read -> close` 读取内置只读文件。

当前限制：

- 当前文件仍来自内核静态只读文件表，不来自真实磁盘。
- 暂不支持写入、create/delete、目录树、权限、inode、block cache、pipe fd、dup/dup2。
- 当前只实现只读普通文件 fd，不完整统一 stdin/stdout/stderr。

## ✅ Task59：用户态 cat 程序 / open-read-close syscall 对接

本轮目标：

- 在 Task58 的 fd 层基础上新增用户态 `cat` 程序。
- 让 `run cat /readme.txt` 通过 syscall 访问内核只读文件。
- 明确区分 shell 内建 `cat` 和用户态 `cat`。

已完成：

- 新增用户态 `cat` 程序镜像，并加入统一 `program_id` / 程序名映射。
- `run cat /readme.txt` 与 `run cat /programs` 均可工作。
- 用户态 `cat` 通过：
  - `SYS_OPEN`
  - `SYS_READ`
  - `SYS_CLOSE`
  读取内置只读文件。
- 用户态 `cat` 通过 `SYS_WRITE` 输出读取结果。
- `/programs` 文件内容已同步加入 `cat`。
- shell 内建 `cat <file>` 继续保留；`run cat <file>` 则走用户态程序链路。

当前限制：

- 当前仍只支持内核静态只读文件，不支持真实磁盘。
- 暂不支持写入、create/delete、目录树、权限、inode、block cache、pipe fd、dup/dup2。
- 用户指针检查仍是教学版最小实现，后续可以继续增强。

## ✅ Task60：用户态 ls 程序 / 文件列表 syscall 对接

本轮目标：

- 新增用户态 `ls` 程序。
- 让 `run ls` 通过 syscall 获取内置只读文件列表。
- 明确区分 shell 内建 `ls` 与用户态 `ls`。

已完成：

- 新增用户态 `ls` 程序镜像，并加入统一 `program_id` / 程序名映射。
- `run ls` 可通过 syscall 获取当前内置文件数量、文件路径和文件大小。
- 新增：
  - `SYS_FILE_COUNT`
  - `SYS_FILE_INFO`
- shell 内建 `ls` 继续保留，因此：
  - `ls` 是 shell 内建命令
  - `run ls` 是用户态程序链路
- `/programs` 文件内容已同步加入 `ls`。

当前限制：

- 当前仍不是真实目录系统，不支持目录树、`readdir/getdents`、权限、inode、block cache、真实磁盘。
- 用户态 `ls` 只支持最小文件名 / 文件大小展示，不支持 `ls -l`、排序或路径参数。

## ✅ Task61：文件 stat syscall / 用户态 stat 程序

本轮目标：

- 新增用户态 `stat` 程序。
- 让 `run stat /readme.txt` 通过 syscall 查询文件元信息。
- 明确区分“列文件列表”“读文件内容”“查单个文件属性”这三类教学版文件接口。

已完成：

- 新增教学版 `SYS_STAT`。
- 新增教学版 `struct minios_stat`，当前只包含：
  - `size`
  - `type`
- 新增用户态 `stat` 程序镜像，并加入统一 `program_id` / 程序名映射。
- `run stat /readme.txt` 与 `run stat /programs` 均可工作。
- 当前文件类型统一显示为 `readonly-file`。
- `/programs` 文件内容已同步加入 `stat`。

当前限制：

- 当前仍不支持 inode、权限位、uid/gid、时间戳、block 数、软链接/硬链接。
- 当前 `stat` 不是完整 POSIX `stat`，只返回教学版最小元信息。

## ✅ Task62：RAMFS 可写内存文件系统雏形 / touch、writefile、rm

本轮目标：

- 在只读内置文件表之外，再新增一张教学版 RAMFS 内存文件表。
- 支持创建、覆盖写入、删除小文本文件。
- 让 `ls/cat/stat` 与 `run ls/run cat/run stat` 都能观察 RAMFS 文件。

已完成：

- 新增 RAMFS 文件表，当前文件全部驻留内存，重启后丢失。
- 新增 shell 命令：
  - `touch <file>`
  - `writefile <file> <text>`
  - `rm <file>`
- `ls` 和 `run ls` 现在都能同时看到只读内置文件与 RAMFS 文件。
- `cat` 和 `run cat` 现在都能读取 RAMFS 文件内容。
- `run stat` 现在能显示 RAMFS 文件的：
  - `Name`
  - `Size`
  - `Type: ramfs-file`
- 内置只读文件仍然禁止 `writefile` 和 `rm`。

当前限制：

- 当前 RAMFS 不持久化，重启后文件丢失。
- 当前只支持小文本文件，不支持真实磁盘、inode、权限、目录树、block cache。
- 当前只支持覆盖写入，不支持 append。
- 当前还没有 `write(fd)`，RAMFS 写入只通过 shell 内建 `writefile`。

## ✅ Task63：RAMFS fd 写入 / write syscall 雏形

本轮目标：

- 把 RAMFS 写入能力从 shell 内建命令推进到 fd / syscall 层。
- 新增用户态 `writefile` 程序，支持 `run writefile /note.txt hello`。
- 保持 shell 内建 `writefile` 继续可用，同时让用户态程序也能通过 syscall 修改 RAMFS 文件。

已完成：

- 新增 `SYS_OPEN_WRITE`，用于以可写方式打开一个已存在的 RAMFS 文件。
- 新增 `SYS_FD_WRITE`，用于通过 fd 向 RAMFS 文件写入文本内容。
- 新增用户态 `writefile` 程序，支持：
  - `run writefile /note.txt hello`
- `run writefile` 现在会通过：
  - `open_write`
  - `fd_write`
  - `close`
  这条链路写入 RAMFS 文件，而不是直接访问内核 RAMFS 表。
- `cat` / `run cat` / `run stat` / `run ls` 均可观察用户态写入后的 RAMFS 文件结果。
- 内置只读文件仍然禁止通过用户态 `writefile` 写入。

当前限制：

- 当前仍不是完整 POSIX `write(fd)` 语义，而是教学版最小写接口。
- 当前只允许写 RAMFS 文件，不允许写内置只读文件。
- 当前只支持覆盖写，append 在下一轮单独补齐。
- 当前不支持真实磁盘、持久化、inode、权限、目录树、并发写锁和复杂 open flags。

## ✅ Task64：RAMFS append 追加写入 / 用户态 append 程序

本轮目标：

- 在已有覆盖写基础上补充 RAMFS append 追加写入语义。
- 新增 shell 内建 `append` 命令。
- 新增用户态 `append` 程序，支持 `run append /note.txt world`。

已完成：

- 新增 shell 内建：
  - `append <file> <text>`
- 新增用户态 `append` 程序，支持：
  - `run append /note.txt world`
- 新增教学版 `SYS_APPEND_FILE`，让用户态程序通过 syscall 追加写入 RAMFS 文件。
- 当前 append 会从文件末尾继续写入，保留旧内容并更新新的 `size`。
- `cat` / `run cat` / `run stat` / `run ls` 均可观察追加后的结果。
- 内置只读文件仍然禁止 append。

当前限制：

- 当前 append 不是完整 POSIX `O_APPEND`。
- 当前不支持并发原子追加、文件锁和 `>>` 重定向。
- 当前仍不支持真实磁盘、持久化、inode、权限和复杂路径解析。

## ✅ Task65：Shell 输出重定向到 RAMFS / > 与 >> 雏形

本轮目标：

- 给教学版 shell 增加最小输出重定向语义。
- 让 `echo text > /file` 走 RAMFS 覆盖写。
- 让 `echo text >> /file` 走 RAMFS 追加写。

已完成：

- shell 已支持：
  - `echo <text> > <file>`
  - `echo <text> >> <file>`
- `>` 会把 echo 输出覆盖写入 RAMFS 文件。
- `>>` 会把 echo 输出追加写入 RAMFS 文件。
- `>` 对不存在文件会自动创建 RAMFS 文件并写入。
- `>>` 要求目标 RAMFS 文件已存在，避免追加语义混淆。
- 内置只读文件仍禁止被 `>` / `>>` 修改。
- `cat` / `run cat` / `run stat` / `run ls` 都可以观察重定向后的结果。

当前限制：

- 当前只支持 `echo` 的输出重定向。
- 暂不支持通用用户程序 stdout 重定向。
- 暂不支持 `<`、`2>`、`2>&1`。
- 暂不支持 `dup/dup2`。
- 暂不支持管道与重定向组合。
- 暂不支持后台任务重定向。
- 暂不支持复杂引号解析。

## ✅ Task66：用户态程序 stdout 重定向到 RAMFS / run ... > file

本轮目标：

- 把 Task65 的 echo 专用重定向推进到 `run` 启动的用户态程序。
- 支持 `run cat /readme.txt > /copy.txt`、`run ls > /files.txt`、`run stat /readme.txt > /stat.txt`。
- 让用户态程序继续调用 `SYS_WRITE`，由内核根据当前进程的 stdout 重定向配置决定输出到屏幕还是 RAMFS。

已完成：

- shell 已支持：
  - `run <program> [args] > <file>`
  - `run <program> [args] >> <file>`
- `>` 的语义是：
  - 第一次 `SYS_WRITE` 覆盖写入目标 RAMFS 文件
  - 后续同一进程的多次 `SYS_WRITE` 自动改为追加，避免只保留最后一段输出
- `>>` 的语义是：
  - 整个进程生命周期内都按追加写入处理
- `run cat /readme.txt > /copy.txt`
  - 当前会把用户态 `cat` 输出完整写入 RAMFS 文件
- `run ls > /files.txt`
  - 当前会把用户态 `ls` 的多段输出完整写入 RAMFS 文件
- `run stat /readme.txt > /stat.txt`
  - 当前会把用户态 `stat` 的完整元信息输出写入 RAMFS 文件
- `SYS_WRITE` 已能根据当前进程的 stdout 重定向配置决定写屏幕还是写 RAMFS。
- Task65 的 `echo > /file` / `echo >> /file` 仍保持原样可用。

当前限制：

- 当前不是完整 `dup2`/fd stdout 重定向模型。
- 暂不支持 `<`、`2>`、`2>&1`。
- 暂不支持管道和重定向组合。
- 暂不支持后台任务重定向。
- 暂不支持多个重定向。
- 暂不支持复杂引号解析。
- 当前仍不支持真实磁盘、持久化、inode、权限和目录树。

## ✅ Task67：用户态程序 stdin 重定向到 RAMFS / run ... < file

本轮目标：

- 把 Task66 的 stdout 重定向继续推进成教学版 stdin 重定向。
- 支持 `run cat < /readme.txt`、`run cat < /programs`、`run cat < /input.txt`。
- 让用户态 `cat` 在没有文件参数时，通过 `SYS_READ(fd=0)` 从文件读取内容。

已完成：

- shell 已支持：
  - `run cat < /readme.txt`
  - `run cat < /programs`
  - `run cat < /input.txt`
- 当前通过在 PCB 里保存 stdin 重定向配置，让 `SYS_READ(fd=0)` 在启用时直接从文件读取。
- 输入源既可以是内置只读文件，也可以是 RAMFS 文件。
- 用户态 `cat` 已增加 stdin 模式：
  - `argc >= 2` 时保持原有 argv 文件模式
  - `argc < 2` 时循环 `SYS_READ(0, ...)` 直到 EOF
- 普通 `run cat /readme.txt` 仍保持原有 open/read/close 路径。
- Task66 的 `run ... > file` stdout 重定向仍保持可用。
- Task65 的 `echo > /file` / `echo >> /file` 仍保持可用。

当前限制：

- 当前不是完整 `dup2`/fd stdin 重定向模型。
- 暂不支持真实 tty 和键盘交互 stdin。
- 暂不支持 here-doc、管道和后台输入重定向。
- 暂不支持多个 `<`。
- 暂不支持或暂不推荐 `<` 与 `>` 组合。
- 暂不支持复杂引号解析。
- 当前仍不支持真实磁盘、持久化、inode、权限和目录树。

## ✅ Task68：组合重定向 < + > 雏形 / run ... < input > output

本轮目标：

- 把 Task66 的 stdout 重定向与 Task67 的 stdin 重定向组合起来。
- 支持 `run cat < /readme.txt > /copy.txt`。
- 让用户态程序形成教学版 `file -> stdin -> stdout -> file` 数据流。

已完成：

- shell 已支持：
  - `run cat < /readme.txt > /copy.txt`
  - `run cat < /input.txt > /output.txt`
  - `run cat < /input.txt >> /log.txt`
- shell 解析时会把 `<` / `>` / `>>` 以及对应路径从用户程序 argv 中剥离。
- shell 创建子进程时可以同时设置：
  - stdin 重定向路径
  - stdout 重定向路径
- `SYS_READ(fd=0)` 继续从输入文件读取。
- `SYS_WRITE` 继续根据 stdout 重定向配置把输出写入 RAMFS。
- 单独的 `<`、`>`、`>>` 行为都保持原样可用。

当前限制：

- 当前不是完整 `dup2`/fd 复制模型。
- 暂不支持管道 `|`。
- 暂不支持 stderr 重定向。
- 暂不支持后台任务重定向。
- 暂不支持多个输入或多个输出重定向。
- 暂不支持复杂引号解析。
- 暂不支持真实磁盘、持久化、inode、权限和目录树。

## ✅ Task69：单管道 | 雏形 / 用户程序 stdout 接 stdin

本轮目标：

- 在 Task66～68 的重定向基础上，支持教学版单管道：
  - `run cat /readme.txt | run cat`
- 让左侧用户程序 stdout 进入内核 pipe buffer，再由右侧用户程序 stdin 读取。

已完成：

- shell 已支持：
  - `run <program> [args] | run <program> [args]`
- 当前只支持单个 `|`，并且左右两侧都必须是 `run` 命令。
- 左侧程序先完整运行，把所有 `SYS_WRITE` 输出写入教学版 pipe buffer。
- 左侧结束后，右侧程序再运行，并通过 `SYS_READ(fd=0)` 从 pipe buffer 读取。
- 当前 `run cat /readme.txt | run cat`
  - 会把 `/readme.txt` 内容显示到屏幕
  - 左侧输出不会额外直接显示一份
- 当前 `run cat /programs | run cat`
  - 会把 `/programs` 内容显示到屏幕
- RAMFS 文件也可以作为左侧输入，例如：
  - `run cat /input.txt | run cat`

当前限制：

- 当前不是完整 UNIX pipe。
- 暂不支持多级管道。
- 暂不支持并发执行两端。
- 暂不支持阻塞 pipe。
- 暂不支持 pipe fd。
- 暂不支持 dup2。
- 暂不支持后台管道。
- 暂不支持管道和重定向组合。
- 暂不支持复杂引号解析。

## ✅ Task70：管道 + 输出重定向组合雏形 / run A | run B > file

本轮目标：

- 在 Task69 单管道基础上，继续支持教学版：
  - `run cat /readme.txt | run cat > /copy.txt`
  - `run cat /programs | run cat > /programs_copy.txt`
  - `run cat /programs | run cat >> /log.txt`
- 让左侧程序 stdout 先进 pipe buffer，再由右侧程序从 pipe buffer 读取并写入 RAMFS 文件。

已完成：

- shell 现在支持：
  - `run <program> [args] | run <program> [args] > <file>`
  - `run <program> [args] | run <program> [args] >> <file>`
- 左侧进程继续启用 `stdout -> pipe`。
- 右侧进程现在可以同时启用：
  - `stdin <- pipe`
  - `stdout -> RAMFS file`
- 当前 `run cat /readme.txt | run cat > /copy.txt`
  - 会把 `/readme.txt` 内容写入 `/copy.txt`
- 当前 `run cat /programs | run cat > /programs_copy.txt`
  - 会把 `/programs` 内容写入目标 RAMFS 文件
- 当前 `run cat /programs | run cat >> /log.txt`
  - 会把右侧输出追加到已有 RAMFS 文件末尾

当前限制：

- 当前仍不是完整 UNIX pipe。
- 暂不支持多级管道。
- 暂不支持并发 pipe。
- 暂不支持阻塞 pipe。
- 暂不支持 pipe fd。
- 暂不支持 dup2。
- 暂不支持 stdin 重定向 + pipe。
- 暂不支持后台管道。
- 暂不支持 stderr 重定向。
- 暂不支持复杂 shell 组合。

## ✅ Task71：管道 + 输入重定向组合雏形 / run A < input | run B

本轮目标：

- 在 Task67 文件 stdin 重定向和 Task69 单管道基础上，继续支持教学版：
  - `run cat < /readme.txt | run cat`
  - `run cat < /programs | run cat`
  - `run cat < /input.txt | run cat`
- 让左侧程序先从文件读取 stdin，再把输出写进 pipe buffer，最后由右侧程序从 pipe buffer 读取并输出到屏幕。

已完成：

- shell 现在支持：
  - `run <program> < <file> | run <program> [args]`
- 左侧进程现在可以同时启用：
  - `stdin <- file`
  - `stdout -> pipe`
- 右侧进程继续启用：
  - `stdin <- pipe`
- 当前 `run cat < /readme.txt | run cat`
  - 会把 `/readme.txt` 内容显示到屏幕
- 当前 `run cat < /programs | run cat`
  - 会把 `/programs` 内容显示到屏幕
- RAMFS 文件也可作为左侧 stdin 输入，例如：
  - `run cat < /input.txt | run cat`

当前限制：

- 当前仍不是完整 UNIX pipe。
- 暂不支持多级管道。
- 暂不支持并发 pipe。
- 暂不支持阻塞 pipe。
- 暂不支持 pipe fd。
- 暂不支持 dup2。
- 暂不支持后台管道。
- 暂不支持复杂 shell 组合。

## ✅ Task72：完整单管道数据流雏形 / run A < input | run B > output

本轮目标：

- 在 Task70 和 Task71 基础上，继续支持教学版完整数据流：
  - `run cat < /readme.txt | run cat > /copy.txt`
  - `run cat < /programs | run cat > /programs_copy.txt`
  - `run cat < /input.txt | run cat > /output.txt`
- 让左侧程序从文件读取 stdin，再把输出写进 pipe buffer，最后由右侧程序从 pipe buffer 读取并写入 RAMFS 文件。

已完成：

- shell 现在支持：
  - `run <program> < <file> | run <program> [args] > <file>`
  - `run <program> < <file> | run <program> [args] >> <file>`
- 左侧进程现在可以同时启用：
  - `stdin <- file`
  - `stdout -> pipe`
- 右侧进程现在可以同时启用：
  - `stdin <- pipe`
  - `stdout -> file`
- 当前 `run cat < /readme.txt | run cat > /copy.txt`
  - 会把 `/readme.txt` 内容写入 `/copy.txt`
- 当前 `run cat < /programs | run cat > /programs_copy.txt`
  - 会把 `/programs` 内容写入目标 RAMFS 文件
- RAMFS 文件也可同时作为输入和输出，例如：
  - `run cat < /input.txt | run cat > /output.txt`

当前限制：

- 当前仍不是完整 UNIX pipe。
- 暂不支持多级管道。
- 暂不支持并发 pipe。
- 暂不支持阻塞 pipe。
- 暂不支持 pipe fd。
- 暂不支持 dup2。
- 暂不支持后台管道。
- 暂不支持 stderr 重定向。
- 暂不支持复杂 shell 组合。

## ✅ Task73：用户态 wc 程序 / stdin 数据流验证

本轮目标：

- 新增一个最小用户态 `wc` 程序，专门验证：
  - `stdin -> 用户程序处理 -> stdout`
  - `stdin <- file`
  - `stdin <- pipe`
  - `stdout -> RAMFS file`

已完成：

- 新增用户态 `wc` 程序，当前通过 `sys_read(0, ...)` 从 stdin 读取数据。
- `wc` 当前输出：
  - `bytes`
  - `lines`
  - `words`
- 当前支持：
  - `run wc`
  - `run wc < /readme.txt`
  - `run cat /readme.txt | run wc`
  - `run cat < /input.txt | run wc > /count.txt`
- `wc` 通过 `sys_write(...)` 输出统计结果，因此可以自然复用：
  - 屏幕输出
  - `stdout` 重定向
  - pipe 输入链路

当前限制：

- 当前仍是教学版 `wc`。
- 暂不支持 Linux `wc` 参数。
- 暂不支持 `-l` / `-w` / `-c`。
- 暂无交互式 tty stdin。
- 暂不支持 `run wc /file` 这种 argv 文件模式；当前统一通过 stdin 读取。

## ✅ Task74：用户态 grep 程序 / pipe 文本过滤验证

本轮目标：

- 新增一个最小用户态 `grep` 程序，用来验证：
  - `stdin -> 用户态按行过滤 -> stdout`
  - `stdin <- file`
  - `stdin <- pipe`
  - `stdout -> RAMFS file`

已完成：

- 新增用户态 `grep` 程序，当前第一个参数作为关键字。
- `grep` 当前通过 `sys_read(0, ...)` 从 stdin 读取数据。
- `grep` 当前通过 `sys_write(...)` 输出“包含关键字的整行”。
- 当前匹配为教学版 ASCII 大小写无关匹配。
- 当前支持：
  - `run grep MiniOS < /readme.txt`
  - `run cat /readme.txt | run grep MiniOS`
  - `run cat < /readme.txt | run grep MiniOS > /grep.txt`
- `grep` 可以验证：
  - 文件 stdin
  - pipe stdin
  - stdout 重定向到 RAMFS 文件

当前限制：

- 当前仍是教学版 `grep`。
- 暂不支持正则表达式。
- 暂不支持 `-i` / `-n` / `-v` 等参数。
- 暂不支持多文件输入。
- 暂无交互式 tty stdin。
- 暂不支持 `run grep keyword /file` 这种 argv 文件模式；当前统一通过 stdin 读取。

## ✅ Task75：用户态 head 程序 / 读取前 N 行

本轮目标：

- 新增一个最小用户态 `head` 程序，用来验证：
  - `stdin -> 用户态按行截断 -> stdout`
  - `stdin <- file`
  - `stdin <- pipe`
  - `stdout -> RAMFS file`

已完成：

- 新增用户态 `head` 程序，默认输出前 `10` 行。
- `head` 当前支持 `-n N` 参数，用于指定输出前 N 行。
- `head` 当前统一通过 `sys_read(0, ...)` 从 stdin 读取数据。
- `head` 当前通过 `sys_write(...)` 输出前 N 行。
- 当前支持：
  - `run head < /readme.txt`
  - `run head -n 3 < /readme.txt`
  - `run cat /readme.txt | run head`
  - `run cat < /readme.txt | run head -n 3 > /head.txt`
- `head` 可以验证：
  - 文件 stdin
  - pipe stdin
  - stdout 重定向到 RAMFS 文件

当前限制：

- 当前仍是教学版 `head`。
- 暂不支持多个文件参数。
- 暂不支持完整 GNU `head` 参数。
- 暂不支持 `run head /file` 这种 argv 文件模式；当前统一通过 stdin 读取。
- 暂不支持真正 UNIX pipe、pipe fd 和 `dup2`。

## ✅ Task76：用户态 tail 程序 / 简化版尾部输出

本轮目标：

- 新增一个最小用户态 `tail` 程序，用来验证：
  - `stdin -> 用户态缓存 -> 尾部行截断 -> stdout`
  - `stdin <- file`
  - `stdin <- pipe`
  - `stdout -> RAMFS file`

已完成：

- 新增用户态 `tail` 程序，默认输出最后 `10` 行。
- `tail` 当前支持 `-n N` 参数，用于指定输出最后 N 行。
- `tail` 当前统一通过 `sys_read(0, ...)` 从 stdin 读取数据。
- `tail` 当前会先缓存固定窗口，再从后往前定位最后 N 行的起始位置。
- 当前支持：
  - `run tail < /readme.txt`
  - `run tail -n 3 < /readme.txt`
  - `run cat /readme.txt | run tail`
  - `run cat < /readme.txt | run tail -n 3 > /tail.txt`
- 当前用户态文本工具链已经包括：
  - `cat / wc / grep / head / tail`
- `tail` 可以继续验证：
  - 文件 stdin
  - pipe stdin
  - stdout 重定向到 RAMFS 文件

当前限制：

- 当前仍是教学版 `tail`。
- 暂不支持多个文件参数。
- 暂不支持完整 GNU `tail` 参数。
- 暂不支持 `-f`。
- 暂不支持 `run tail /file` 这种 argv 文件模式；当前统一通过 stdin 读取。
- 使用固定缓冲区，只保证窗口范围内的最后 N 行。
- 暂不支持真正 UNIX pipe、pipe fd 和 `dup2`。

TODO：

- 可继续推进 `sort` 等更复杂的用户态文本工具。
- 可进一步整理 pipe buffer 容量限制与错误提示。

## ✅ Task77：用户态 sort 程序 / 小输入行排序

本轮目标：

- 新增一个最小用户态 `sort` 程序，用来验证：
  - `stdin -> 用户态缓存 -> 按行切分 -> 排序 -> stdout`
  - `stdin <- file`
  - `stdin <- pipe`
  - `stdout -> RAMFS file`

已完成：

- 新增用户态 `sort` 程序，当前按行做字典序升序排序。
- `sort` 当前统一通过 `sys_read(0, ...)` 从 stdin 读取全部输入。
- `sort` 当前在用户态内部使用固定缓冲区和固定行表，先切分文本行，再做简单排序。
- 当前支持：
  - `run sort < /readme.txt`
  - `run cat /readme.txt | run sort`
  - `run cat < /readme.txt | run sort > /sorted.txt`
- 当前用户态文本工具链已经包括：
  - `cat / wc / grep / head / tail / sort`
- `sort` 可以继续验证：
  - 文件 stdin
  - pipe stdin
  - stdout 重定向到 RAMFS 文件

当前限制：

- 当前仍是教学版 `sort`。
- 暂不支持多个文件参数。
- 暂不支持完整 GNU `sort` 参数。
- 暂不支持 `-r`、`-n`、去重和外部排序。
- 暂不支持 `run sort /file` 这种 argv 文件模式；当前统一通过 stdin 读取。
- 使用固定缓冲区和固定最大行数；超限时返回简单错误提示。
- 暂不支持真正 UNIX pipe、pipe fd 和 `dup2`。

TODO：

- 可继续整理 pipe buffer 的容量限制与错误处理。
- 可继续推进真正的 pipe fd / `dup2` 雏形。
- 可补充一组 Phase2 数据流演示脚本与讲解文档。

## ✅ Task78：pipe buffer 容量限制与错误处理整理

本轮目标：

- 不新增用户态程序，只整理教学版单管道 `pipe buffer` 的边界行为。
- 明确当前 pipe 是“左侧先写完，右侧再读取”的顺序模型。
- 明确固定容量、满写行为、空读 / EOF 语义，以及每次命令前后的初始化与清理。

已完成：

- 补清楚教学版 `pipe buffer` 的状态语义：`active / size / read_offset / overflowed`。
- 明确当前固定容量为 `512` 字节。
- 每次执行 `run A | run B` 前后仍会显式清空 pipe 状态，避免残留旧数据。
- 左侧程序写 pipe 时会先检查剩余空间，不再允许越界写入。
- 当 pipe 写满时，当前策略是：
  - 尽量把剩余空间写满
  - 只输出一次 `pipe: buffer full`
  - 后续写入返回 `0`，不 panic
- 右侧程序从 pipe 读取时：
  - `read_offset >= size` 返回 `0`
  - 空 pipe 或未激活 pipe 也返回 `0`
  - 统一表现为最小 EOF 语义
- 当前教学版数据流链路仍然是：
  - 文件系统 -> stdin/stdout -> redirect -> pipe -> `cat / wc / grep / head / tail / sort`

当前限制：

- 不支持真正 UNIX pipe
- 不支持 pipe fd
- 不支持 `dup2`
- 不支持阻塞读写
- 不支持并发 pipe
- 不支持多级管道
- 不支持动态扩容 pipe buffer

TODO：

- 可继续进入 Task79：真正 pipe fd 雏形
- 可继续整理 shell parser 和错误提示
- 可补一组 Phase2 数据流演示脚本

## ✅ Task79：真正 pipe fd 雏形

本轮目标：

- 不新增用户态程序，而是让教学版 pipe 开始进入 fd 体系。
- 引入最小的 `pipe read fd / pipe write fd` 概念。
- 让 `sys_read / sys_write` 开始能按 fd 类型分发到普通文件或 pipe。

已完成：

- fd 表现在可以区分：
  - 普通文件 fd
  - `pipe read fd`
  - `pipe write fd`
- 当前仍然只保留一个教学版全局 pipe buffer，但左右端已经能通过 fd 类型与它建立关系。
- shell 执行 `run A | run B` 时：
  - 左侧程序内部绑定一个 `pipe write fd`
  - 右侧程序内部绑定一个 `pipe read fd`
- `SYS_READ(fd, ...)` 当前已经能识别 `pipe read fd`，并从 pipe buffer 读取。
- `SYS_FD_WRITE(fd, ...)` 与 `SYS_WRITE(...)` 当前已经能识别 `pipe write fd`，并把内容写入 pipe buffer。
- 对 `pipe write fd` 调用读会返回错误；对 `pipe read fd` 调用写也会返回错误。
- pipe 写满仍沿用 Task78：
  - 固定容量 `512` 字节
  - 尽量写满剩余空间
  - 只提示一次 `pipe: buffer full`
  - 后续返回 `0`
- 当前数据流链路已经更接近：
  - 文件系统 -> fd -> stdin/stdout -> redirect -> pipe fd -> `cat / wc / grep / head / tail / sort`

当前限制：

- 不支持用户态 `pipe()`
- 不支持 `dup2`
- 不支持 fork 后共享 pipe fd
- 不支持阻塞读写
- 不支持并发 pipe
- 不支持多级管道
- 不支持多个 pipe object
- 当前仍然保留少量兼容字段，后续还可继续向统一 fd 抽象收口

TODO：

- 可继续做 Task80：fd 抽象清理
- 可继续做 `dup2` 雏形
- 可继续做 pipe syscall 雏形
- 可继续整理 shell 数据流演示文档

## ✅ Task80：fd 抽象整理 / 统一 file fd 与 pipe fd 分发路径

本轮目标：

- 不新增用户态程序，而是整理 fd 抽象。
- 统一普通文件 fd 与 pipe fd 的查找、分配、重置和读写分发路径。
- 让 `sys_read / sys_write` 的结构更清楚，给后续 `dup2` / `pipe()` / fd 继承打基础。

已完成：

- fd 类型定义和注释进一步整理清楚。
- 当前 fd 类型至少包括：
  - 普通文件 fd
  - `pipe read fd`
  - `pipe write fd`
- 增加了更明确的教学版 fd 辅助逻辑：
  - fd 编号转槽位
  - fd 表项查询
  - fd 槽位重置
  - fd 空闲槽位分配
- `SYS_READ(fd, ...)` 的分发路径更清楚：
  - `fd=0` 先走教学版 stdin 兼容入口
  - `FD_FILE` -> 文件读取
  - `FD_PIPE_READ` -> pipe 读取
  - `FD_PIPE_WRITE` -> 错误
- `SYS_FD_WRITE(fd, ...)` 的分发路径更清楚：
  - `FD_FILE` -> 文件写入
  - `FD_PIPE_WRITE` -> pipe 写入
  - `FD_PIPE_READ` -> 错误
- pipe stdin / stdout 已经优先通过绑定的 `pipe read fd / pipe write fd` 进入统一分发。
- `stdin_redirect_from_pipe` / `stdout_redirect_to_pipe` 仍然保留为兼容字段，但已经不再是主要分发依据。
- 普通文件 fd、stdin/stdout redirect、教学版 pipe、RAMFS 文本工具链保持兼容目标不变。

当前限制：

- 不支持 `dup2`
- 不支持用户态 `pipe()`
- 不支持 fork 后共享 fd
- 不支持引用计数
- 不支持阻塞 pipe
- 不支持并发 pipe
- 不支持多个 pipe object
- 不支持多级管道

TODO：

- Task81 可继续做 `dup2` 雏形
- 可继续做 pipe syscall 雏形
- 可继续整理 shell 数据流路径

## ✅ Task81：dup2 雏形 / fd 重定向统一入口

本轮目标：

- 不新增用户态程序。
- 新增内核内部 `fd_dup2(oldfd, newfd)` 雏形。
- 为 stdin/stdout redirect 和 pipe 提供更统一的“把 oldfd 接到 0/1”的入口。

已完成：

- 新增教学版内核内部 `fd_dup2(oldfd, newfd)`。
- 当前 `fd_dup2` 可以检查：
  - `oldfd` 是否有效
  - `newfd` 是否有效
  - `oldfd == newfd`
  - `newfd` 先清空再覆盖
- 当前 `fd_dup2` 支持复制：
  - 普通文件 fd
  - `pipe read fd`
  - `pipe write fd`
- 当前 `pipe` 配置路径已经开始迁移：
  - 左侧通过 `fd_dup2(pipe_write_fd, 1)` 接到 stdout
  - 右侧通过 `fd_dup2(pipe_read_fd, 0)` 接到 stdin
- 文件型 stdin/stdout redirect 当前仍保留兼容路径，没有强行一次性迁移。
- 当前 `fd_dup2` 仍不是完整 POSIX 语义：
  - 没有引用计数
  - 没有 fork 后共享 fd
  - 没有 close-on-exec
  - `newfd=1` 绑定文件时仍沿用教学版 stdout 重定向语义

当前限制：

- 不支持用户态 `dup2` syscall
- 不支持引用计数
- 不支持 fork 后共享 fd
- 不支持 close-on-exec
- 不支持完整 POSIX `dup2` 错误语义
- 不支持并发安全
- 不支持多个 pipe object
- pipe 仍然是教学版顺序 pipe

TODO：

- Task82 可继续迁移 shell 文件重定向到 dup2 路径
- 可继续做用户态 `dup2` syscall
- 可继续做 `pipe()` syscall 雏形
- 可继续整理 fork 后 fd 继承语义

## ✅ Task82：Shell 重定向迁移到 dup2 路径

本轮目标：

- 不新增用户态程序。
- 把 Shell 的 `<` / `>` / `< + >` 文件重定向开始迁移到 `fd_dup2` 路径。
- pipe 连接留到 Task83 继续统一。

已完成：

- `run A < input`
  - 已开始优先走：
    - 打开输入文件 fd
    - `fd_dup2(input_fd, 0)`
- `run A > output`
  - 已开始优先走：
    - 创建或打开输出文件 fd
    - `fd_dup2(output_fd, 1)`
- `run A < input > output`
  - 已开始分别设置 `fd=0` 和 `fd=1`
- 当前实现仍然保留 stdout/stderr 兼容字段和教学版写文件逻辑，
  这样可以在不破坏现有行为的前提下，先把 Shell 接线入口迁到 dup2。
- pipe 本轮不做额外收口，继续由后续 Task83 统一整理。

当前限制：

- 不支持用户态 `dup2` syscall
- 不支持完整 POSIX open flags
- 不支持引用计数
- 不支持 fork 后共享 fd
- pipe 暂时仍走兼容路径
- 不是完整 UNIX shell 重定向实现

TODO：

- Task83 可继续迁移 pipe 到 dup2 路径
- 可继续整理 shell 数据流状态清理
- 可继续做用户态 `dup2` syscall

## ✅ Task83：pipe 迁移到 dup2 路径

本轮目标：

- 不新增用户态程序。
- 把 shell 的 `run A | run B` 连接明确统一到 `fd_dup2` 路径。
- 保持当前仍然是教学版顺序 pipe，而不是并发 UNIX pipe。

已完成：

- shell 左侧 pipe 连接现在明确采用：
  - 分配 `pipe write fd`
  - `fd_dup2(pipe_write_fd, 1)`
- shell 右侧 pipe 连接现在明确采用：
  - 分配 `pipe read fd`
  - `fd_dup2(pipe_read_fd, 0)`
- `run A | run B`、`run A | run B > output`、`run A < input | run B`、`run A < input | run B > output`
  继续沿用同一套 shell 启动路径：
  - 左侧先运行并写 pipe
  - 右侧后运行并读 pipe
- pipe buffer 容量、EOF、写满单次提示等边界行为继续沿用 Task78。
- `stdout_redirect_to_pipe` / `stdin_redirect_from_pipe` 仍保留为兼容字段，
  但 shell pipe 的主要接线入口已经统一到 `fd_dup2`。

当前限制：

- 不支持用户态 `pipe()` syscall
- 不支持用户态 `dup2()` syscall
- 不支持 fork 后共享 pipe fd
- 不支持引用计数
- 不支持阻塞读写
- 不支持并发 pipe
- 不支持多级管道
- 不支持多个 pipe object
- 仍然不是完整 UNIX shell pipe

TODO：

- Task84 可继续清理 `fd=0/1` 的教学版特殊入口
- 可继续减少 `stdout_redirect_to_pipe` / `stdin_redirect_from_pipe` 兼容字段参与度
- 可继续做用户态 `dup2()` 或 `pipe()` syscall 雏形

## ✅ Task84：pipe() syscall 雏形 / 用户态创建 pipe fd

本轮目标：

- 不改变现有 shell `run A | run B` 行为。
- 新增最小用户态 `pipe()` syscall。
- 让用户程序可以显式拿到一对 pipe fd 做最小读写验证。

已完成：

- 新增 `SYS_PIPE`
- 新增内核 `process_pipe_create_fds(int* user_fds)`
- 成功时返回：
  - `fds[0] = pipe read fd`
  - `fds[1] = pipe write fd`
- 继续沿用当前唯一的全局教学版 pipe buffer
- 新增用户态 `pipe_test` 程序，用来验证：
  - `pipe(fds)` 创建成功
  - `write(fds[1], ...)` 能写 pipe
  - `read(fds[0], ...)` 能读 pipe
  - 再次读取会返回最小 EOF

当前限制：

- 不支持用户态 `dup2()` syscall
- 不支持多个独立 pipe object
- 不支持阻塞读写
- 不支持并发 pipe
- 不支持多级管道
- 不支持 fork 后共享 pipe fd

TODO：

- Task85 可继续清理 pipe fd 生命周期
- 可继续做用户态 `dup2()` syscall 雏形
- 可继续推进更真实的 pipe object 模型

## ✅ Task85：dup2 syscall 雏形 / 用户态 fd 重定向能力

本轮目标：

- 不改变现有 shell pipe / redirect 行为。
- 把内核内部 `fd_dup2` 暴露成最小用户态 `dup2(oldfd, newfd)` syscall。
- 新增用户态 `dup2_test` 程序验证 pipe fd 的复制与读写。

已完成：

- 新增 `SYS_DUP2`
- 新增内核 `process_dup2(oldfd, newfd)`，内部复用既有 `fd_dup2`
- 成功时返回 `newfd`
- 失败时统一返回 `-1`
- `oldfd == newfd` 时稳定返回 `newfd`
- 新增用户态 `dup2_test` 程序，当前采用“复制到普通 fd=5/6 再读写”的测试方案，避免覆盖 `stdout`
- `dup2_test` 可以验证：
  - `dup2(pipe_write_fd, 5)` 后通过 `write(5, ...)` 写入 pipe
  - `dup2(pipe_read_fd, 6)` 后通过 `read(6, ...)` 读取 pipe
  - `dup2(valid_fd, valid_fd)` 的稳定返回
  - `dup2(-1, 5)` / `dup2(valid_fd, -1)` / `dup2(valid_fd, 99)` 的最小错误路径

当前限制：

- 仍然不是完整 POSIX `dup2`
- 不支持引用计数
- 不支持 close-on-exec
- 不支持 fork 后 fd 共享
- 不支持并发 pipe
- 不支持阻塞 pipe
- 不支持完整错误码
- 当前 `newfd >= 3` 仍是教学版“表项复制”，不是共享同一个 file object

TODO：

- Task86 可继续推进 fork 后 fd 继承语义
- Task87 可继续做用户态 `fork + pipe + dup2` 组合实验
- Task88 可继续探索并发 pipe / 阻塞读写雏形

## ✅ Task86：fork 后 fd 继承语义整理

本轮目标：

- 整理 fork 时子进程如何继承父进程 fd table。
- 让子进程继承普通文件 fd、pipe fd，以及当前 0/1 对应的教学版 stdin/stdout 绑定关系。
- 新增用户态 `fork_fd_test`，验证“父进程建 pipe -> fork -> 子进程写 -> 父进程 wait 后读”链路。

已完成：

- fork 路径新增 `process_copy_fd_table(child, parent)`
- 子进程现在会继承：
  - 普通文件 fd
  - pipe read fd
  - pipe write fd
  - `stdin_redirect_*`
  - `stdout_redirect_*`
  - `stdin_pipe_fd / stdout_pipe_fd`
- 新增用户态 `fork_fd_test` 程序，用来验证：
  - 父进程先创建 pipe
  - `fork()` 后子进程继承写端
  - 子进程写入 `child says hello\n`
  - 父进程 `waitpid` 后从读端读回数据
- 子进程退出不会主动清空全局 pipe buffer，因此父进程仍可在 wait 之后读取数据

当前限制：

- 仍然不是完整 POSIX fork fd 继承
- 不支持引用计数
- 子进程继承当前采用教学版浅拷贝 / 视图复制
- 文件 fd 仍不是共享同一个底层 file object
- pipe 仍然只有一个全局教学版缓冲区
- 不支持并发 pipe / 阻塞 pipe / 多个 pipe object

TODO：

- Task87 可继续做用户态 `fork + pipe + dup2` 组合验证
- Task88 可继续探索并发 pipe / 阻塞读写雏形
- 后续可继续整理更真实的 file object / 引用计数模型

## ✅ Task87：用户态 pipe + fork + dup2 组合测试

本轮目标：

- 不新增复杂内核机制。
- 新增用户态 `pipe_fork_dup2_test` 程序。
- 验证用户态已经可以自己组合：
  - `pipe()`
  - `fork()`
  - `dup2()`
  - `read/write`

已完成：

- 新增用户态 `pipe_fork_dup2_test`
- 当前测试链路是：
  - 父进程 `pipe(fds)`
  - 父进程 `fork()`
  - 子进程 `dup2(fds[1], 1)`
  - 子进程通过 `write(1)` 把文本写入 pipe
  - 父进程 `waitpid()`
  - 父进程 `dup2(fds[0], 0)`
  - 父进程通过 `read(0)` 读回子进程写入的内容
- 这说明：
  - `pipe()` 已经可由用户态显式创建
  - `fork()` 已经能继承 pipe fd
  - `dup2()` 已经能把 pipe 端点接到 `0/1`
  - 用户态已能跑出最小“接近真实 UNIX 管道模型”的闭环

当前限制：

- 当前没有 `exec`
- 当前不是并发阻塞 pipe
- 当前仍然只有一个全局教学版 pipe buffer
- 当前不是完整 UNIX pipeline
- 当前不支持多个并发 pipe object

TODO：

- Task88 可继续做 pipe 读端/写端关闭语义
- Task89 可继续探索并发 pipe / 阻塞读写雏形
- 后续可继续做用户态 `fork + exec + pipe + dup2` 组合测试
