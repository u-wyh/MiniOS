# Task57：内核内置只读文件表 / ls、cat 雏形

## 1. 本轮目标

本轮目标是先引入文件系统语义的最小雏形：在内核中维护一张只读文件表，并让 shell 支持 `ls` 和 `cat <file>`。

## 2. 为什么需要本任务

后续如果要做 `open/read/close` syscall，必须先回答两个问题：

1. 文件名如何映射到文件对象
2. 文件对象如何保存大小和内容

所以 Task57 先不做真实磁盘，而是先做教学版只读文件表。

## 3. 当前文件表结构

当前把只读文本文件抽象成：

- `path`
- `content`
- `size`

文件内容直接来自内核静态只读字符串。

## 4. ls 语义

`ls` 当前只列出内核内置只读文件，不做真实目录遍历。

输出至少包含：

- 文件名
- 文件大小

## 5. cat 语义

`cat <file>` 当前只支持读取一份内置只读文件并直接输出内容。

为了降低教学环境里的输入门槛，当前还兼容：

- `/readme.txt`
- `readme.txt`
- `readmetxt`

这类等价输入形式。

当前不支持：

- 多文件 cat
- 重定向
- 路径规范化
- 写入

## 6. 当前内置文件

当前实现包含：

- `/readme.txt`
- `/programs`
- `/help.txt`

## 7. 当前限制

1. 暂不支持真实磁盘
2. 暂不支持目录树
3. 暂不支持写入
4. 暂不支持 `open/read/close` syscall
5. 暂不支持权限系统
6. 暂不支持 inode
7. 暂不支持 block cache
8. 后续可扩展 fd 表和 `read` syscall

## 8. 验证方式

```text
ls
cat /readme.txt
cat /programs
cat /not_exist.txt
```
