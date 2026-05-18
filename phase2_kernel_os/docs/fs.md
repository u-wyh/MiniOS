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

`cat <file>` 当前直接输出指定内置只读文件内容。

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

## 6. 当前限制

1. 当前只支持只读普通文件
2. 暂不支持写入
3. 暂不支持 create/delete
4. 暂不支持 pipe fd
5. 暂不支持 dup/dup2
6. 暂不完整支持 stdin/stdout/stderr
7. 暂不支持真实磁盘与目录树
