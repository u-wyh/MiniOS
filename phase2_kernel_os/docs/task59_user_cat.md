# Task59：用户态 cat 程序 / open-read-close syscall 对接

## 1. 本轮目标

本轮目标是让用户态 `cat` 程序通过 syscall 读取内置只读文件，而不是直接访问内核文件表。

## 2. 为什么需要本任务

Task57 和 Task58 已经建立了内核内置文件表与 fd 抽象，但真正的操作系统隔离语义还需要用户程序通过 syscall 请求文件服务。

## 3. 用户态 cat 运行链路

```text
run cat /readme.txt
    -> exec cat
        -> SYS_OPEN
            -> SYS_READ
                -> SYS_WRITE
                    -> SYS_CLOSE
```

## 4. open/read/close syscall

- `SYS_OPEN(path)`
  - 成功返回 fd
  - 失败返回负值
- `SYS_READ(fd, buf, size)`
  - 成功返回读取字节数
  - EOF 返回 `0`
  - 失败返回负值
- `SYS_CLOSE(fd)`
  - 成功返回 `0`
  - 失败返回负值

## 5. fd 与用户程序

fd 不是文件本身，而是用户程序打开文件后拿到的句柄。  
fd 表项当前最小记录：

- `used`
- `file`
- `offset`

## 6. 与 shell 内建 cat 的区别

```text
cat /readme.txt
    -> shell 内建命令

run cat /readme.txt
    -> 用户态 cat 程序
```

## 7. 验证方式

```text
ls
cat /readme.txt
run cat /readme.txt
run cat /programs
run cat /not_exist.txt
run cat
```

## 8. 当前限制

1. 暂不支持真实磁盘
2. 暂不支持写入
3. 暂不支持 create/delete
4. 暂不支持目录树
5. 暂不支持权限
6. 暂不支持 inode
7. 暂不支持 block cache
8. 暂不支持 pipe fd
9. 暂不支持 dup/dup2
10. 暂不支持重定向
11. 用户指针检查仍是教学版最小实现
12. 后续可以继续扩展更多用户态文件测试程序与更完整的文件 syscall
