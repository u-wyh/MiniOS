# Task20：系统调用层

## 1. syscall layer 是什么

syscall layer 指的是：

- 用户态统一通过一个入口进入内核
- 内核再根据 syscall 编号分发不同服务

它和“只做一个 write 演示”的区别在于：

- 入口还是同一个 `int 0x80`
- 但内核不再只做一件事
- 而是开始具备“按编号分流”的能力

## 2. 为什么需要 syscall 表 / 分发层

如果每个功能都单独写一套入口：

- 扩展困难
- 维护混乱
- 不符合真实操作系统的设计方式

所以更合理的做法是：

1. 用户态统一执行 `int 0x80`
2. `eax` 放 syscall 编号
3. 内核 handler 读取编号并分发

这就是最小 syscall layer 的意义。

## 3. 参数传递机制

当前约定如下：

- `eax`：syscall 编号
- `ebx`：第一个参数

本轮 syscall 编号：

```c
#define SYS_WRITE   1
#define SYS_EXIT    2
#define SYS_GETPID  3
#define SYS_TIME    4
```

当前只有 `SYS_WRITE` 真正使用了参数：

- `ebx = str_ptr`

也就是把用户态字符串地址传给内核。

## 4. 返回值机制

当前最小返回值约定是：

- 内核把结果放回 `eax`

例如：

- `SYS_GETPID` 返回 `pid`
- `SYS_TIME` 返回 `pit_get_ticks()`

本轮为了便于观察，内核还会直接把这些值打印到屏幕上。

## 5. 当前支持的 syscall

### 5.1 `SYS_WRITE`

作用：

- 输出用户态字符串

形式：

```text
eax = 1
ebx = str_ptr
int 0x80
```

### 5.2 `SYS_EXIT`

作用：

- 表示当前用户态测试程序结束

本轮最小实现里，它会触发“测试收口”，让系统停在内核态，便于观察结果。

### 5.3 `SYS_GETPID`

作用：

- 返回当前最小 pid

当前实现返回固定 `1`，作为最小验证版本。

### 5.4 `SYS_TIME`

作用：

- 返回当前 PIT tick

这说明 syscall 不再只是打印固定字符串，而是开始读取真实内核状态。

## 6. 执行流程

当前完整链路如下：

```text
shell 输入 user
-> 内核进入 Ring3
-> 用户程序依次执行 write/getpid/time/exit
-> 每次都通过 int 0x80 进入内核
-> syscall_handle 根据 eax 分发
-> 内核执行对应服务
-> 返回用户态或在 exit 后停机收口
```

## 7. 关键代码解释

### 7.1 `syscall_handle`

这是最小 syscall layer 的核心。

职责：

- 读取 `frame->eax`
- 判断 syscall 编号
- 执行对应逻辑
- 把返回值写回 `frame->eax`

### 7.2 `interrupt_handler_80`

它是 `int 0x80` 进入 C 层后的统一入口。

本轮职责：

- 判断这次中断是否来自 Ring3
- 若来自用户态，就把现场交给 `syscall_handle`
- 若收到 `SYS_EXIT`，则在内核态停机收口

### 7.3 `SYS_TIME`

它直接读取：

- `pit_get_ticks()`

这说明 syscall 现在已经可以访问真实内核运行状态。

### 7.4 `SYS_GETPID`

当前是最小实现：

- 返回固定 pid `1`

后续如果任务/进程模型继续扩展，这里可以再接到真正的任务结构。

## 8. 当前限制

本轮仍然有这些限制：

- 还没有真正的 syscall 表数组
- 还没有用户指针合法性检查
- `SYS_GETPID` 还是固定返回
- `SYS_EXIT` 还不是“优雅回到 shell”，而是测试收口
- 没有文件系统，所以 `write` 不是写文件，而是直接写 VGA 输出

## 9. 常见错误

### 9.1 只改了用户态程序，没改内核分发

结果：

- `eax` 传了编号，但内核不认识

### 9.2 忘了把返回值写回 `eax`

结果：

- 用户态得不到 syscall 结果

### 9.3 `SYS_TIME` 返回固定值

结果：

- 这就不是真正的系统状态读取

### 9.4 `exit` 后没有收口

结果：

- 用户态可能继续落入死循环，干扰测试观察

## 10. 一句话总结

Task20 让 MiniOS 从“只有一个 write syscall”升级为“具备最小 syscall 分发层”的内核。
