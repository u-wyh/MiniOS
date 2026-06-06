# Task61：文件 stat syscall / 用户态 stat 程序

## 1. 本轮目标

本轮目标是支持用户态 `stat` 查询文件元信息。

## 2. 为什么需要本任务

文件系统不仅需要读取内容和列出文件，还需要查询单个文件的属性。

Task57~Task60 已经覆盖了：

- `ls`：列出文件
- `cat`：读取文件

因此本轮继续补上：

- `stat`：查询单个文件元信息

## 3. stat syscall 语义

当前采用教学版最小接口：

```text
sys_stat(path, stat_buf) -> 0 或负值
```

其中：

- `path`：用户态路径字符串
- `stat_buf`：用户态 `struct minios_stat*`
- 成功返回 `0`
- 失败返回负值

## 4. stat 结构体

当前只支持最小字段：

- `size`
- `type`

当前 `type` 只定义了一种：

- `readonly-file`

## 5. 用户态 stat 运行链路

```text
run stat /readme.txt
    -> exec stat
        -> sys_stat
            -> fs_find / fs_builtin_file_stat
                -> 返回 size/type
                    -> sys_write 输出
```

## 6. 与 ls/cat 的关系

- `ls`：列出文件列表
- `cat`：读取文件内容
- `stat`：查询单个文件元信息

它们现在共同组成了教学版最小文件接口闭环。

## 7. 当前限制

1. 暂不支持真实磁盘
2. 暂不支持目录树
3. 暂不支持 inode
4. 暂不支持权限位
5. 暂不支持 uid/gid
6. 暂不支持 atime/mtime/ctime
7. 暂不支持 block 数
8. 暂不支持软链接/硬链接
9. 暂不支持完整 POSIX `stat`
10. 用户指针检查仍为教学版

## 8. 验证方式

```text
run stat /readme.txt
run stat /programs
run stat /not_exist.txt
run stat
run ls
run cat /readme.txt
```
