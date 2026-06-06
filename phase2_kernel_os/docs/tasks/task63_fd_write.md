# Task63：RAMFS fd 写入 / write syscall 雏形

## 1. 本轮目标

本轮目标是把 RAMFS 写入能力从 shell 内建命令继续推进到 fd / syscall 层，让用户态程序也能写 RAMFS 文件。

## 2. 为什么需要本任务

Task62 已经支持：

- `touch <file>`
- `writefile <file> <text>`
- `rm <file>`

但当时写入能力主要还停留在 shell 内建命令直接调用 RAMFS 接口。  
本轮继续把这部分能力暴露给用户态程序，形成更完整的“用户态 -> syscall -> fd -> 文件对象”链路。

## 3. 当前写入链路

```text
run writefile /note.txt hello
    -> exec writefile
        -> sys_open_write(path)
            -> 返回可写 fd
                -> sys_fd_write(fd, text, size)
                    -> RAMFS content 更新
                        -> sys_close(fd)
```

## 4. fd 写入语义

当前教学版 fd 写入语义如下：

1. 只有 RAMFS 文件可以通过可写 fd 打开
2. 内置只读文件不能通过 fd 写入
3. 打开可写 fd 时 offset 从 0 开始
4. 当前采用覆盖写语义，不支持 append
5. 写入后会更新文件大小
6. 如果新内容更短，旧尾巴会被清理，避免残留脏数据

## 5. 用户态 writefile

当前新增用户态程序：

```text
run writefile /note.txt hello
```

它会：

1. 检查参数
2. 调用 `sys_open_write`
3. 调用 `sys_fd_write`
4. 调用 `sys_close`
5. 正常退出或打印最小错误信息

## 6. 与 shell 内建 writefile 的区别

```text
writefile /note.txt hello
```

- shell 内建命令

```text
run writefile /note.txt hello
```

- 用户态程序，通过 syscall/fd 写入

两者当前都会修改同一份 RAMFS 文件内容，但用户态程序不能直接访问内核 RAMFS 表。

## 7. 只读文件保护

像 `/readme.txt`、`/programs`、`/help.txt` 这样的内置只读文件：

1. 不能通过 shell 内建 `writefile` 修改
2. 也不能通过 `run writefile` 写入
3. `sys_open_write` 会直接拒绝这类只读目标

## 8. 当前限制

1. 暂不支持真实磁盘
2. 暂不支持持久化
3. 暂不支持 inode
4. 暂不支持权限系统
5. 暂不支持目录树
6. 暂不支持 append
7. 暂不支持复杂 open flags
8. 暂不支持并发写锁
9. 暂不支持 pipe fd
10. 暂不支持 dup/dup2
11. 暂不支持重定向
12. 当前用户态 `writefile` 只支持简单文本参数，不做复杂引号解析

## 9. 验证方式

典型验证命令：

```text
touch /note.txt
run writefile /note.txt hello
cat /note.txt
run cat /note.txt
run stat /note.txt
run writefile /readme.txt test
```
