# Task27：父子进程关系与 waitpid 雏形

## 1. 本任务目标

本轮为进程引入父子关系，让 `wait` 的语义更接近真实操作系统：父进程只能回收自己的子进程。

Task26 中，`wait` 会扫描并回收任意一个 ZOMBIE 进程。Task27 后，每个 PCB 都记录 `parent_pid`，`wait` 和 `waitpid` 都必须检查这个归属关系。

## 2. 核心知识点

父进程是创建另一个进程的进程。

子进程是被某个父进程创建出来的进程。

`parent_pid` 的作用是记录“这个进程是谁创建的”。它不是调度字段，而是进程生命周期管理字段。

`wait` 只应该回收子进程，是因为退出状态属于父进程需要观察的结果。如果任意进程都能回收别人的 ZOMBIE，进程归属关系就会混乱。

`wait` 和 `waitpid` 的区别：

- `wait`：回收当前父进程名下任意一个已经退出的子进程。
- `waitpid`：只尝试回收指定 PID 的子进程。

本轮 `waitpid` 不阻塞。如果目标子进程还没退出，直接返回“child still running”，不会让 shell 卡住等待。

本轮不实现 `fork`，因为 `fork` 需要复制地址空间、用户栈和寄存器现场。Task27 只先建立父子关系字段，为后续 `fork` 铺路。

## 3. 当前进程关系模型

当前 MiniOS 还没有真正的 init 进程。

因此，从 kernel monitor / shell 里执行 `run hello` 创建出来的进程，暂时使用：

```text
parent_pid = 0
```

未来如果实现 `fork`，子进程会记录：

```text
parent_pid = 当前进程 pid
```

`ps` 通过 `PPID` 展示父子关系：

```text
PID   PPID   STATE
1     0      ZOMBIE
```

## 4. 执行流程

```text
run hello
-> process_create
-> 设置 parent_pid
-> process_run
-> 用户程序 exit
-> int 0x80
-> syscall handler
-> process_exit
-> 进程变 ZOMBIE
-> shell wait / waitpid
-> 检查 parent_pid
-> process_wait / process_waitpid
-> 回收子进程 PCB
```

## 5. 关键代码解释

`process_create(name)`：

- 从 ramfs 查找 ELF 文件。
- 分配一个 PCB。
- 加载 ELF。
- 分配 PID。
- 根据当前上下文设置 `parent_pid`。
- shell / kernel monitor 创建进程时，父 PID 暂时为 0。

`process_wait()`：

- 取得当前父进程 PID。
- 扫描进程表。
- 只回收 `state == PROCESS_ZOMBIE` 且 `parent_pid` 匹配的进程。
- 回收成功返回 pid，否则返回 `-1`。

`process_waitpid(pid)`：

- 先按 pid 查找进程。
- 找不到返回 `-1`。
- 找到了但不是当前父进程的子进程，返回 `-2`。
- 是子进程但还不是 ZOMBIE，返回 `-3`。
- 是 ZOMBIE 子进程则清空 PCB，并返回 pid。

`ps`：

- 跳过 `PROCESS_UNUSED` 槽位。
- 显示 `PID / PPID / STATE / STATUS / NAME`。
- `PPID` 来自 PCB 的 `parent_pid` 字段。

shell 中的 `waitpid <pid>`：

- 使用项目内的最小数字解析函数读取 pid。
- 不依赖 `atoi` 或 libc。
- 根据 `process_waitpid` 的返回值输出不同提示。

## 6. 当前限制

- 还没有 fork。
- 还没有真正 init 进程。
- `waitpid` 不阻塞。
- 没有父子进程树。
- 没有进程组。
- 没有完整资源释放。
- kernel monitor 下 `parent_pid` 暂时用 0 表示。

## 7. 常见错误

- `wait` 回收了非子进程。
- `waitpid` 没检查 `parent_pid`。
- `ps` 不显示 `PPID`。
- `current_process` 为空时访问崩溃。
- `waitpid` 解析 pid 依赖 `atoi` 导致链接失败。
- 回收后 PCB 没有变回 `PROCESS_UNUSED`。

## 8. 一句话总结

父子进程关系让 MiniOS 的进程回收从“全局扫描”升级为“按归属关系管理”。
