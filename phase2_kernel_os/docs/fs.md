# MiniOS Phase2 文件系统雏形

## 1. 当前定位

当前 MiniOS 还没有真实磁盘文件系统。

本阶段的“文件”来自内核静态只读数据，主要用于让 shell 先具备最小的：

- `ls`
- `cat <file>`

语义。

## 2. 内置只读文件表

当前内核维护一张统一的内置只读文本文件表。

每个文件至少包含：

- `path`
- `content`
- `size`

当前内置文件包括：

- `/readme.txt`
- `/programs`
- `/help.txt`

## 3. ls / cat

### ls

`ls` 当前只列出内核内置只读文件：

- 文件名
- 文件大小

它不做真实目录遍历。

### cat

`cat <file>` 当前输出指定内置只读文件内容。

为了降低教学环境里的输入门槛，当前还兼容：

- `/readme.txt`
- `readme.txt`
- `readmetxt`

这类等价输入形式。

## 4. 与真实文件系统的区别

当前实现还不是完整文件系统：

1. 不支持真实磁盘
2. 不支持目录树
3. 不支持写入
4. 不支持权限
5. 不支持 inode
6. 不支持 block cache
7. 不支持持久化

## 5. open / read / close 雏形

在 Task58 中，当前已经补上了教学版只读 fd 层：

```text
path
    -> readonly file object
        -> fd table
            -> open/read/close
```

当前 fd 表项最小记录：

- `used`
- `file`
- `offset`

当前语义：

- `open(path)`：根据路径查找内置只读文件，成功返回 fd
- `read(fd, buf, size)`：从当前 offset 开始读取，读完后 offset 前进
- `close(fd)`：释放 fd 表项，关闭后 fd 失效

当前 fd 从 `3` 开始分配，`0/1/2` 仅保留给后续更完整的标准输入输出语义。

## 6. 用户态 cat 与文件 syscall

在 Task59 中，文件访问链路继续往用户态推进：

```text
run cat /readme.txt
    -> exec 用户态 cat
        -> SYS_OPEN
            -> SYS_READ
                -> SYS_WRITE
                    -> SYS_CLOSE
```

这里要区分两种 `cat`：

- `cat /readme.txt`
  - shell 内建命令
- `run cat /readme.txt`
  - 用户态 `cat` 程序

用户态 `cat` 不能直接访问内核文件表，只能通过 syscall 拿到 fd 并循环读取。

## 7. 当前限制

1. 当前只支持只读普通文件
2. 暂不支持写入
3. 暂不支持 create/delete
4. 暂不支持 pipe fd
5. 暂不支持 dup/dup2
6. 暂不完整支持 stdin/stdout/stderr
7. 暂不支持真实磁盘与目录树
8. 用户指针检查仍是教学版最小实现

## 8. 用户态 ls 与文件列表 syscall

在 Task60 中，MiniOS 继续把文件系统语义从 shell 内建命令推进到用户态程序：

```text
run ls
    -> exec 用户态 ls
        -> SYS_FILE_COUNT
        -> SYS_FILE_INFO
        -> SYS_WRITE
```

这里也要区分两种 `ls`：

- `ls`
  - shell 内建命令
- `run ls`
  - 用户态 `ls` 程序

当前新增的文件列表 syscall 只服务于内置只读文件表，不是完整目录接口：

1. `SYS_FILE_COUNT()`：返回当前内置文件数量
2. `SYS_FILE_INFO(index, buf, max_len)`：复制指定文件路径到用户缓冲区，并返回文件大小

这一步的意义是让“列出文件元信息”也开始走用户态 syscall 路径，而不只是停留在 shell 内建实现中。

## 9. 用户态 stat 与文件元信息 syscall

在 Task61 中，MiniOS 继续把“查询单个文件属性”暴露给用户态程序：

```text
run stat /readme.txt
    -> exec 用户态 stat
        -> SYS_STAT(path, stat_buf)
            -> fs_builtin_file_stat(path, out)
                -> 返回 size/type
```

当前教学版 `stat` 结构只包含两项：

- `size`
- `type`

当前文件类型统一定义为：

- `readonly-file`

也就是说，当前 `stat` 不是 POSIX `stat`，只是教学版“单个文件元信息查询接口”。

## 10. Task62：RAMFS 可写内存文件系统雏形

在 Task62 中，MiniOS 继续把文件系统从“只读文件表”推进到“运行时可写内存文件”。

当前新增的是教学版 RAMFS：

1. 文件驻留在内存中
2. 系统重启后全部丢失
3. 当前只支持小文本文件
4. 当前不支持真实磁盘、inode、block cache 或持久化

当前 RAMFS 文件槽位最小记录：

- `used`
- `path`
- `content`
- `size`

当前实现的最小 shell 命令：

- `touch <file>`：创建空 RAMFS 文件
- `writefile <file> <text>`：覆盖写入文本内容
- `rm <file>`：删除 RAMFS 文件

当前文件列表 syscall 现在服务于“当前可见文件列表”，也就是：

1. 内置只读文件
2. RAMFS 内存文件

因此 `ls` / `run ls`、`cat` / `run cat`、`run stat` 都能看到 RAMFS 文件。

当前 `stat` 的类型也扩展为：

- `readonly-file`
- `ramfs-file`

## 11. 当前限制

1. 暂不支持真实磁盘
2. 暂不支持持久化
3. 暂不支持 inode
4. 暂不支持权限系统
5. 暂不支持目录树
6. 暂不支持 block cache
7. 暂不支持 append
8. 暂不支持 `write(fd)`
9. 暂不支持复杂路径解析
10. 暂不支持多进程并发写保护
