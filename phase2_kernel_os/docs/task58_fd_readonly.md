# Task58：只读文件描述符 / open-read-close syscall 雏形

## 1. 本轮目标

本轮目标是从内置文件表继续推进到 fd 抽象。

## 2. 为什么需要本任务

真实 OS 不是直接用“文件名 -> 内容”工作，而是通过 fd 访问打开的文件。  
fd 是后续 `read/write/pipe/dup` 的基础。

## 3. fd 是什么

fd 不是文件本身，而是一个“打开文件后的句柄”。

文件对象描述：

- 文件名
- 文件内容
- 文件大小

fd 表项描述：

- 这个槽位是否占用
- 当前打开的是哪个文件
- 当前读到哪个 offset

## 4. fd 表结构

当前最小表项包含：

- `used`
- `file`
- `offset`

当前实现为每进程 fd 表。

## 5. open 语义

`open(path)` 的最小路径是：

```text
path
    -> fs_builtin_file_find(path)
        -> 分配 fd 表项
            -> offset = 0
                -> 返回 fd
```

当前 fd 从 `3` 开始分配。

## 6. read 语义

`read(fd, buf, size)`：

- 从当前 offset 开始读取
- 成功返回实际读取字节数
- 读到 EOF 返回 `0`
- 读取后 offset 前进

## 7. close 语义

`close(fd)`：

- 释放 fd 表项
- 关闭后 fd 失效
- 之后再 `read(fd)` 应安全失败

## 8. cat 与 fd 层

当前 `cat <file>` 已优先通过：

```text
open(path)
    -> read(fd, buf, size)
        -> close(fd)
```

读取内置只读文件。

## 9. 当前限制

1. 暂不支持真实磁盘
2. 暂不支持写入
3. 暂不支持 create/delete
4. 暂不支持目录树
5. 暂不支持权限
6. 暂不支持 inode
7. 暂不支持 block cache
8. 暂不支持 pipe fd
9. 暂不支持 dup/dup2
10. 暂不完整支持 stdin/stdout/stderr
11. 后续可扩展用户态更完整的文件接口和用户态 cat 程序

## 10. 验证方式

```text
ls
cat /readme.txt
cat /programs
cat /not_exist.txt
cat /readme.txt
cat /readme.txt
fdtest
```
