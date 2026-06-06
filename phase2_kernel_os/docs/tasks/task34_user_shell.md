# Task34：最小用户态 Shell 雏形

## 1. shell 为什么也是普通用户进程？

在操作系统语义里，shell 并不是“内核的一部分”，而是运行在用户态的普通进程。

它之所以看起来特殊，不是因为它拥有特殊特权，而是因为它承担了“帮用户启动别的程序”的职责。

当前 MiniOS 的 Task34 也是沿着这个方向实现的：

- `shell` 通过普通用户态 syscall 工作
- `shell` 自己也会 `exit`
- `shell` 自己也会被父进程 `waitpid` 回收

## 2. init 和 shell 的关系是什么？

在当前教学版实现里，关系是：

`init -> shell`

也就是：

- `init` 是第一个用户进程
- `init` fork 出一个子进程
- 这个子进程再 exec 成 `shell`

所以 `shell` 不是内核直接创建的长期特殊对象，而是 `init` 的一个普通子进程。

## 3. shell 执行程序为什么需要 fork / exec / waitpid？

因为 shell 自己通常不能直接“变成”要执行的程序，否则它本身就没了。

所以经典路径是：

1. shell `fork`
2. 子进程 `exec`
3. shell `waitpid`

当前 MiniOS 的脚本式 shell 也是这么做的：

- `shell` 自己先 `fork`
- 子进程 `exec` 成 `hello`
- `shell` 等待并回收这个子进程

## 4. 当前 MiniOS shell 为什么是固定脚本式？

因为这一轮目标是先把用户态层次搭出来，而不是一次性做完整交互 shell。

所以当前版本故意简化成：

- 启动后输出 `shell start`
- 输出一条固定脚本提示，例如 `MiniOS$ run hello`
- 自动执行一条固定命令
- 回收子进程后退出

这样能最小代价验证：

- `shell` 确实在用户态运行
- `shell` 确实能用 `fork/exec/waitpid`
- 父子关系确实从 `init -> shell -> hello` 串起来

## 5. init -> shell -> user program 的父子关系如何形成？

当前父子链是这样形成的：

1. 内核启动 `init`
2. `init` fork，生成子进程
3. 该子进程 exec 成 `shell`
4. `shell` 再 fork，生成自己的子进程
5. 该子进程 exec 成 `hello`

因此会形成：

- `shell.parent_pid == init.pid`
- `hello.parent_pid == shell.pid`

也就是最小用户态层次：

`init -> shell -> hello`

## 6. 当前 shell 和 Phase1 用户态 shell 有什么关系？

Phase1 的 shell 更像是“概念和功能演示”，很多逻辑可以直接用普通函数调用完成。

Task34 的 shell 则更偏向真正的 OS 语义：

- 它本身是一个用户进程
- 它通过 syscall 进入内核
- 它通过 `fork/exec/waitpid` 启动和回收别的进程

所以可以把它理解成：

- Phase1 shell：功能原型
- Phase2 Task34 shell：进程语义原型

## 7. 后续要做交互式 shell 还缺哪些能力？

如果要把当前脚本式 shell 推进成真正交互式 shell，至少还需要：

- 键盘输入接入用户态
- `stdin/stdout` 语义
- 命令解析
- 路径字符串 `exec`
- `argv/envp`
- 更完整的用户空间与文件系统配合

Task34 先完成的是“让 shell 成为普通用户进程，并能通过 `fork/exec/waitpid` 管理子程序”这一步。
