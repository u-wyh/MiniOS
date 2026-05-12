# MiniOS Phase1：Linux 用户态操作系统机制模拟实验

## 1. 项目简介

MiniOS Phase1 是一个运行在 Linux 用户态的教学型操作系统机制模拟项目。

本阶段并不是实现一个真正运行在内核态的操作系统内核，而是在 Linux 用户态中实现一个类 Shell 程序，并在该 Shell 进程内部模拟操作系统中的若干核心机制，包括：

- 命令解释与执行
- 外部程序运行
- 管道与重定向
- 后台任务管理
- TCB 任务表
- Round-Robin 调度器
- 信号量同步机制
- 简单内存管理
- 内存型文件系统 MiniFS

Phase1 的主要目标是：在进入 QEMU 裸机内核开发之前，先通过 Linux 用户态程序理解操作系统中的进程、调度、同步、内存和文件系统等核心概念。

---

## 2. 项目定位

本项目可以分为两部分理解：

### 2.1 用户态 Shell 功能

MiniOS Phase1 首先是一个用户态 Shell。它负责：

- 读取用户输入
- 解析命令参数
- 执行内建命令
- 调用 Linux 系统命令
- 支持管道
- 支持输入 / 输出重定向
- 支持后台任务

这部分更接近真实 Linux Shell 的职责。

### 2.2 操作系统机制模拟

在 Shell 的基础上，项目进一步在用户态模拟了一些本来通常属于操作系统内核的机制，例如：

- 任务状态管理
- 调度队列
- 信号量等待队列
- 内存块分配表
- 简单文件系统结构

这些模块并不会真正替代 Linux 内核的调度器、内存管理器或文件系统，而是通过 C++ 数据结构模拟这些机制的基本思想。

因此，Phase1 的准确定位是：

> 一个运行在 Linux 用户态的 MiniOS 教学模拟环境。它以 Shell 为入口，一部分实现真实 Shell 能力，另一部分在用户态模拟操作系统核心机制，为后续裸机内核实验做准备。

---

## 3. 项目目录结构

```text
phase1/
├── include/
│   ├── commands.h
│   ├── fs.h
│   ├── memory.h
│   ├── scheduler.h
│   ├── semaphore.h
│   ├── shell.h
│   └── task.h
│
├── src/
│   ├── commands.cpp
│   ├── fs.cpp
│   ├── main.cpp
│   ├── memory.cpp
│   ├── scheduler.cpp
│   ├── semaphore.cpp
│   ├── shell.cpp
│   └── task.cpp
│
├── Makefile
└── README.md
```

---

## 4. 编译与运行

### 4.1 编译项目

在 `phase1` 目录下执行：

```bash
make
```

编译成功后会生成可执行文件：

```text
MiniOS
```

### 4.2 运行项目

```bash
make run
```

或者手动运行：

```bash
./MiniOS
```

进入后会看到 MiniOS Shell 提示符：

```text
MiniOS>
```

### 4.3 清理编译结果

```bash
make clean
```

### 4.4 重新编译

```bash
make rebuild
```

---

## 5. Makefile

本项目使用 Makefile 管理编译流程。

```makefile
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude

TARGET := MiniOS

SRC := \
	src/main.cpp \
	src/shell.cpp \
	src/commands.cpp \
	src/task.cpp \
	src/scheduler.cpp \
	src/semaphore.cpp \
	src/memory.cpp \
	src/fs.cpp

.PHONY: all run clean rebuild

all:
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET)

rebuild: clean all
```

---

## 6. 模块说明

### 6.1 main 模块

`main.cpp` 是程序入口。

它只负责创建 Shell 对象并启动 Shell 主循环，保持入口逻辑简洁。

核心流程：

```text
main()
  ↓
创建 Shell
  ↓
调用 Shell::run()
  ↓
进入 MiniOS 命令循环
```

---

### 6.2 Shell 模块

相关文件：

```text
include/shell.h
src/shell.cpp
```

Shell 模块负责：

- 输出命令提示符
- 读取用户输入
- 将输入拆分为 tokens
- 按顺序分发不同类型命令
- 非阻塞回收后台子进程
- 控制主循环退出

Shell 的命令分发顺序大致为：

```text
后台命令
  ↓
管道 + 输出重定向
  ↓
输入重定向
  ↓
输出重定向
  ↓
普通管道
  ↓
内建命令
  ↓
外部命令
```

这样可以避免复杂命令被普通命令提前误处理。

---

### 6.3 Commands 模块

相关文件：

```text
include/commands.h
src/commands.cpp
```

Commands 模块负责具体命令执行，包括：

#### 内建命令

```text
help
pwd
cd
echo
clear
exit
host
ls
```

#### 外部命令

当输入不是 MiniOS 内建命令时，会尝试通过 Linux 外部程序执行。

例如：

```bash
uname -a
date
whoami
```

外部命令执行流程：

```text
fork()
  ↓
子进程 execvp()
  ↓
父进程 waitpid()
```

这部分用于理解 Linux 中 Shell 执行外部程序的基本机制。

#### 管道

支持单管道命令：

```bash
ls | grep cpp
```

其核心思想是：

```text
pipe()
  ↓
左命令 stdout 重定向到管道写端
  ↓
右命令 stdin 重定向到管道读端
  ↓
两个子进程分别 execvp()
```

#### 输出重定向

支持：

```bash
echo hello > out.txt
echo world >> out.txt
```

其中：

- `>` 表示覆盖写入
- `>>` 表示追加写入

#### 输入重定向

支持：

```bash
cat < input.txt
```

其本质是将文件描述符重定向到标准输入。

#### 后台命令

支持：

```bash
sleep 30 &
run sleep 30 &
```

后台命令不会阻塞 MiniOS Shell，而是立即返回提示符。

---

### 6.4 Task 任务管理模块

相关文件：

```text
include/task.h
src/task.cpp
```

Task 模块用于模拟操作系统中的任务管理机制。

项目中定义了一个简化版 TCB：

```cpp
struct TCB {
    int tid;
    pid_t hostPid;
    std::string command;
    TaskState state;
};
```

其中：

| 字段 | 含义 |
|---|---|
| `tid` | MiniOS 内部任务编号 |
| `hostPid` | Linux 宿主系统中的进程 PID |
| `command` | 任务对应的命令字符串 |
| `state` | MiniOS 内部维护的任务状态 |

任务状态包括：

```text
Ready
Running
Blocked
Done
Killed
Failed
```

支持命令：

```bash
run <command> &
ps
kill <tid>
block <tid>
wake <tid>
```

说明：

- `run <command> &` 创建一个受 MiniOS 管理的后台任务
- `ps` 查看 MiniOS 内部任务表
- `kill <tid>` 终止指定任务
- `block <tid>` 将任务标记为阻塞
- `wake <tid>` 唤醒阻塞任务

需要注意的是，MiniOS 的 TCB 是用户态模拟数据结构，不等价于 Linux 内核中的 `task_struct`。

---

### 6.5 Scheduler 调度器模块

相关文件：

```text
include/scheduler.h
src/scheduler.cpp
```

Scheduler 模块用于模拟一个简单的 Round-Robin 调度器。

核心数据包括：

```text
currentTid
readyQueue
policy
```

支持命令：

```bash
sched status
sched tick
sched policy rr
```

#### sched status

查看当前调度器状态：

```bash
sched status
```

输出内容包括：

- 当前调度策略
- 当前运行任务
- 就绪队列内容

#### sched tick

手动推进一次调度：

```bash
sched tick
```

它会从 ready queue 中选择下一个任务，并更新 MiniOS 内部任务状态。

需要注意的是：

> Phase1 的调度器不会真正控制 CPU，也不会替代 Linux 内核调度器。它只是改变 MiniOS 内部维护的任务状态和 ready queue，用于理解调度机制。

---

### 6.6 Semaphore 信号量模块

相关文件：

```text
include/semaphore.h
src/semaphore.cpp
```

Semaphore 模块用于模拟操作系统中的同步机制。

信号量结构：

```cpp
struct Semaphore {
    std::string name;
    int count;
    std::queue<int> waitQueue;
};
```

支持命令：

```bash
sem create <name> <count>
sem wait <name>
sem post <name>
sem list
```

#### sem create

创建信号量：

```bash
sem create mutex 1
```

#### sem wait

申请资源：

```bash
sem wait mutex
```

如果信号量 count 大于 0，则 count 减 1。

如果 count 等于 0，则当前任务会进入等待队列，并被标记为 Blocked。

#### sem post

释放资源：

```bash
sem post mutex
```

如果等待队列中有任务，则唤醒一个等待任务。

#### sem list

查看当前所有信号量：

```bash
sem list
```

该模块用于理解：

- 资源计数
- wait / post 操作
- 阻塞队列
- 任务唤醒

---

### 6.7 Memory 内存管理模块

相关文件：

```text
include/memory.h
src/memory.cpp
```

Memory 模块用于模拟一个简单的内存池。

支持命令：

```bash
mem alloc <size>
mem free <id>
mem stat
```

#### mem alloc

申请一块逻辑内存：

```bash
mem alloc 128
```

成功后返回 block id。

#### mem free

释放指定内存块：

```bash
mem free 1
```

#### mem stat

查看内存状态：

```bash
mem stat
```

输出内容包括：

- 总容量
- 已使用空间
- 剩余空间
- 每个内存块的状态

说明：

> Phase1 的 MemoryManager 并不管理真实物理内存或虚拟内存，而是在用户态维护一个逻辑内存池，用于模拟内存分配和释放过程。

---

### 6.8 MiniFS 文件系统模块

相关文件：

```text
include/fs.h
src/fs.cpp
```

MiniFS 是一个简单的内存型文件系统。

文件结构：

```cpp
struct FileNode {
    std::string name;
    std::string content;
};
```

支持命令：

```bash
touch <file>
write <file> <text>
cat <file>
ls
stat <file>
rm <file>
```

#### touch

创建文件：

```bash
touch a.txt
```

#### write

写入文件内容：

```bash
write a.txt hello MiniOS
```

#### cat

查看文件内容：

```bash
cat a.txt
```

#### ls

列出 MiniFS 中的所有文件：

```bash
ls
```

#### stat

查看文件元信息：

```bash
stat a.txt
```

#### rm

删除文件：

```bash
rm a.txt
```

说明：

> MiniFS 不会把数据持久化到真实磁盘。它只是维护在 MiniOS 进程内存中的文件表，用于模拟文件创建、写入、读取、删除和元信息查看。

如果希望查看 Linux 宿主机真实目录，可以使用：

```bash
hostls
```

---

## 7. 支持的命令总览

### 7.1 Shell 基础命令

| 命令 | 作用 |
|---|---|
| `help` | 查看帮助信息 |
| `pwd` | 显示当前工作目录 |
| `cd <path>` | 切换当前工作目录 |
| `echo <text>` | 输出文本 |
| `clear` | 清屏 |
| `exit` | 退出 MiniOS |
| `hostls` | 调用宿主 Linux 的 ls |

### 7.2 外部命令

| 示例 | 作用 |
|---|---|
| `uname -a` | 执行 Linux 外部命令 |
| `date` | 显示系统时间 |
| `whoami` | 显示当前用户 |

### 7.3 管道与重定向

| 命令 | 作用 |
|---|---|
| `cmd1 \| cmd2` | 单管道 |
| `cmd > file` | 输出重定向，覆盖写 |
| `cmd >> file` | 输出重定向，追加写 |
| `cmd < file` | 输入重定向 |
| `cmd1 \| cmd2 > file` | 管道加输出重定向 |

### 7.4 任务管理

| 命令 | 作用 |
|---|---|
| `run <command> &` | 创建受 MiniOS 管理的后台任务 |
| `ps` | 查看 MiniOS 任务表 |
| `kill <tid>` | 终止任务 |
| `block <tid>` | 阻塞任务 |
| `wake <tid>` | 唤醒任务 |

### 7.5 调度器

| 命令 | 作用 |
|---|---|
| `sched status` | 查看调度器状态 |
| `sched tick` | 推进一次 RR 调度 |
| `sched policy rr` | 设置调度策略为 RR |

### 7.6 信号量

| 命令 | 作用 |
|---|---|
| `sem create <name> <count>` | 创建信号量 |
| `sem wait <name>` | 申请资源 |
| `sem post <name>` | 释放资源 |
| `sem list` | 查看信号量列表 |

### 7.7 内存管理

| 命令 | 作用 |
|---|---|
| `mem alloc <size>` | 分配逻辑内存块 |
| `mem free <id>` | 释放逻辑内存块 |
| `mem stat` | 查看内存状态 |

### 7.8 MiniFS 文件系统

| 命令 | 作用 |
|---|---|
| `touch <file>` | 创建文件 |
| `write <file> <text>` | 写入文件 |
| `cat <file>` | 查看文件内容 |
| `ls` | 列出 MiniFS 文件 |
| `stat <file>` | 查看文件元信息 |
| `rm <file>` | 删除文件 |

---

## 8. 示例运行

### 8.1 基础 Shell 命令

```bash
MiniOS> help
MiniOS> pwd
MiniOS> echo hello MiniOS
MiniOS> uname -a
```

### 8.2 管道与重定向

```bash
MiniOS> echo hello > out.txt
MiniOS> cat < out.txt
MiniOS> ls | grep cpp
MiniOS> ls | grep cpp > result.txt
```

### 8.3 后台任务与任务管理

```bash
MiniOS> run sleep 30 &
[started task] id=1 pid=12345

MiniOS> ps
TID   PID     STATE     COMMAND
1     12345   Ready     sleep 30

MiniOS> sched tick
Scheduled task: 1

MiniOS> ps
TID   PID     STATE     COMMAND
1     12345   Running   sleep 30
```

### 8.4 信号量

```bash
MiniOS> sem create mutex 1
MiniOS> sem wait mutex
MiniOS> sem list
MiniOS> sem post mutex
```

### 8.5 内存管理

```bash
MiniOS> mem alloc 128
allocated block id=1 size=128

MiniOS> mem stat
Total: 1024
Used : 128
Free : 896

MiniOS> mem free 1
freed block id=1
```

### 8.6 MiniFS

```bash
MiniOS> touch a.txt
MiniOS> write a.txt hello MiniOS
MiniOS> cat a.txt
hello MiniOS

MiniOS> stat a.txt
MiniOS> ls
MiniOS> rm a.txt
```

---

## 9. Phase1 与真实操作系统的区别

Phase1 是用户态模拟项目，不是真正的内核。

### 9.1 Shell 是用户态程序

MiniOS Shell 和 bash 类似，是运行在 Linux 用户态的普通进程。

它通过 Linux 系统调用使用真实内核能力，例如：

```text
fork
execvp
waitpid
pipe
dup2
open
kill
```

### 9.2 TCB 是模拟的

MiniOS 内部维护的 TCB 只是 C++ 数据结构。

它不等价于 Linux 内核中的进程控制块，也不会替代 Linux 的真实进程管理。

### 9.3 调度器是模拟的

`sched tick` 只会修改 MiniOS 内部的任务状态和 ready queue。

它不会真正决定 CPU 运行哪个 Linux 进程。

### 9.4 内存管理是模拟的

MemoryManager 管理的是一个逻辑内存池。

它不涉及：

- 物理页
- 虚拟地址空间
- 页表
- 缺页异常
- 内核堆管理

### 9.5 文件系统是模拟的

MiniFS 是进程内存中的文件表。

它不涉及：

- 磁盘块
- inode
- 目录项
- 文件权限
- 持久化存储
- 设备驱动

---

## 10. 项目学习价值

通过 Phase1，可以理解以下操作系统核心概念：

### 10.1 Shell 与进程创建

理解 Shell 如何通过 `fork + exec + waitpid` 执行外部程序。

### 10.2 文件描述符与重定向

理解标准输入、标准输出以及 `dup2` 的作用。

### 10.3 管道通信

理解管道如何连接两个进程的输入输出。

### 10.4 后台任务与进程回收

理解后台任务为什么需要非阻塞 `waitpid(WNOHANG)` 回收，避免僵尸进程。

### 10.5 任务状态模型

理解 Ready、Running、Blocked、Done 等任务状态的含义。

### 10.6 调度队列

理解 Round-Robin 调度中的 ready queue 和 current task。

### 10.7 同步机制

理解信号量中的资源计数、等待队列和任务唤醒。

### 10.8 内存管理抽象

理解内存块分配、释放、复用和统计。

### 10.9 文件系统抽象

理解文件创建、写入、读取、删除、列目录和元信息查询的基本模型。

---

## 11. 与 Phase2 的关系

MiniOS 项目分阶段推进：

```text
Phase1：Linux 用户态机制模拟
Phase2：QEMU 裸机内核实验
```

Phase1 的作用是先在 Linux 用户态中熟悉操作系统的基本模块和概念。

Phase2 则进一步进入裸机环境，实现更接近真实内核的内容，例如：

- Multiboot 启动
- GDT / IDT
- 中断处理
- PIC / PIT
- 系统调用
- 分页
- 用户态切换
- 基础任务切换

因此，Phase1 是 Phase2 的概念铺垫和工程准备。

---

## 12. 当前阶段总结

MiniOS Phase1 当前已经实现了一个较完整的用户态 OS 机制模拟环境，覆盖了 Shell、任务、调度、同步、内存和文件系统等多个操作系统核心主题。

该阶段的重点不是追求工业级完整性，而是通过可运行、可验证的代码，把操作系统课本中的抽象概念落到具体实现中，为后续裸机内核开发打基础。
