# MiniOS 项目前期规划总纲

## 一、项目定位

MiniOS 是一个分阶段推进的教学型操作系统项目，目标不是复刻真实大型操作系统，而是通过亲手实现核心机制，系统性理解计算机科学底层运行逻辑。

核心学习闭环：

* 编译器：程序如何被翻译
* 操作系统：程序如何被管理与运行
* 体系结构：程序如何被硬件执行

---

## 二、项目总目标

完成一个具备操作系统核心思想的 MiniOS，逐步实现：

1. 进程/任务管理
2. 调度系统
3. 内存管理
4. 文件系统抽象
5. Shell 命令系统
6. 系统调用机制
7. 中断与时钟驱动（后期）
8. 裸机内核运行（QEMU）

---

## 三、整体三阶段路线

## Phase 1：用户态 MiniOS（当前起点）

开发环境：Linux（云服务器优先）

目标：先吃透操作系统机制，不纠结硬件细节。

实现内容：

* PCB（进程控制块）
* READY / RUNNING / BLOCKED 状态机
* Round Robin 调度器
* 优先级调度器（进阶）
* 时间片模拟
* Semaphore / Mutex
* 页式内存模拟
* 简化文件系统
* Shell（ps / kill / run / mem / top）
* 日志系统

产出结果：

一个可交互命令行 MiniOS。

---

## Phase 2：裸机 MiniOS（QEMU）

开发环境：Linux 虚拟机（推荐）

目标：理解 OS 与硬件接口。

实现内容：

* Bootloader
* Kernel Entry
* VGA 文本输出
* GDT / IDT
* Timer Interrupt
* Keyboard Interrupt
* 简易任务切换
* 页表 Paging
* Syscall 基础机制

产出结果：

在 QEMU 中启动自己的内核。

---

## Phase 3：对照真实 Linux 内核

开发环境：Linux VM / 云服务器均可。

目标：将自己实现的 MiniOS 与工业级系统对照学习。

学习方向：

* 调度器（CFS）
* 内存管理（Buddy / Slab）
* VFS
* System Call Path
* /proc 与系统统计接口
* epoll 与事件驱动

产出结果：

理解真实 Linux 内核设计思想。

---

## 四、当前阶段详细规划（Phase 1）

## 第一模块：项目骨架搭建

目录结构建议：

```text
MiniOS/
├── include/
├── src/
│   ├── kernel/
│   ├── scheduler/
│   ├── memory/
│   ├── fs/
│   ├── shell/
│   └── utils/
├── tests/
├── docs/
├── Makefile
└── README.md
```

---

## 第二模块：进程系统

实现：

* PID 分配
* PCB 类设计
* 状态切换
* 生命周期管理

建议数据结构：

* queue（就绪队列）
* vector（任务表）
* map（PID 查询）

---

## 第三模块：调度器

先做：

* FCFS
  n- Round Robin

后做：

* Priority Scheduler
* MLFQ（挑战项）

统计信息：

* context switch 次数
* 平均等待时间
* CPU 使用率

---

## 第四模块：同步机制

实现：

* Mutex
* Semaphore
* Producer Consumer Demo

---

## 第五模块：内存管理

实现：

* 固定分区分配
* 页式分配
* LRU 页面置换（后期）

输出：

```text
Total Pages: 128
Used Pages : 37
Free Pages : 91
```

---

## 第六模块：文件系统

实现虚拟目录树：

```text
/
├── bin
├── home
└── tmp
```

命令：

* ls
* mkdir
* touch
* cat
* write
* rm

---

## 第七模块：Shell

支持命令：

```bash
help
ps
run test
kill 1
mem
top
ls
exit
```

---

## 五、工程规范

## Git 提交规范

```text
init: initialize MiniOS project
feat: add PCB system
feat: implement RR scheduler
feat: add memory manager
fix: resolve deadlock issue
refactor: optimize task queue design
```

---

## 编译规范

开发模式：

```bash
g++ -std=c++17 -O0 -g
```

发布模式：

```bash
g++ -std=c++17 -O2
```

---

## 六、学习重点（不是代码量）

必须真正理解：

1. 为什么需要调度器
2. 为什么阻塞任务不能继续占 CPU
3. 为什么内存需要分页
4. 为什么文件系统是树状抽象
5. 为什么系统调用要切换权限级别
6. 为什么真实系统要统计资源使用情况

---

## 七、阶段验收标准

## Phase 1 完成标准

* 能启动 MiniOS Shell
* 能创建多个任务
* 能进行时间片调度
* 能 kill 任务
* 能查看内存使用
* 能操作虚拟文件系统
* 代码结构清晰

---

## 八、预计周期（可调整）

* 第1周：骨架 + PCB
* 第2周：调度器
* 第3周：同步机制
* 第4周：内存管理
* 第5周：文件系统
* 第6周：Shell 整合
* 第7周：重构与文档

---

## 九、你的最终项目竞争力

完成后可形成项目组合：

* MiniLang（编译器）
* MiniOS（操作系统）
* 高性能服务器（系统开发）

这是一套很强的底层路线。

---

## 十、当前建议

本周先不开发，只做：

1. 阅读本规划
2. 思考模块设计
3. 准备 Linux 开发环境
4. 明确 Git 仓库结构
5. 下周正式开工
