# Task60：用户态 ls 程序 / 文件列表 syscall 对接

## 1. 本轮目标

本轮目标是让用户态 `ls` 程序通过 syscall 获取文件列表，而不是只依赖 shell 内建 `ls`。

## 2. 为什么需要本任务

Task57 先让 shell 能看到内置只读文件，Task58/59 又把文件读取链路推进到了 `open/read/close` 和用户态 `cat`。

但文件系统语义不只有“读文件内容”，还包括“列出文件元信息”。因此本轮把文件列表能力也暴露给用户态程序。

## 3. 用户态 ls 运行链路

```text
run ls
    -> exec ls
        -> SYS_FILE_COUNT
            -> SYS_FILE_INFO
                -> SYS_WRITE 输出
```

## 4. 文件列表 syscall

当前新增了教学版最小接口：

1. `SYS_FILE_COUNT()`
   - 返回当前内置只读文件数量
2. `SYS_FILE_INFO(index, buf, max_len)`
   - 把指定索引文件路径复制到用户缓冲区
   - 成功时返回该文件大小
   - 失败时返回负值

## 5. 与 shell 内建 ls 的区别

```text
ls
    -> shell 内建命令

run ls
    -> 用户态 ls 程序
```

shell 内建 `ls` 仍然保留；本轮只是额外补上用户态版本，方便后续继续扩展更多文件 syscall 使用场景。

## 6. 当前限制

1. 暂不支持真实磁盘
2. 暂不支持目录树
3. 暂不支持 `readdir/getdents`
4. 暂不支持权限
5. 暂不支持 inode
6. 暂不支持 block cache
7. 暂不支持 `ls -l`
8. 暂不支持路径参数
9. 用户指针检查仍为教学版
10. 后续可以继续扩展更接近真实目录接口的用户态程序

## 7. 验证方式

```text
ls
run ls
cat /readme.txt
run cat /readme.txt
run cat /programs
```
