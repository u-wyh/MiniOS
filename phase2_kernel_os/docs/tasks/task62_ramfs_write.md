# Task62：RAMFS 可写内存文件系统雏形 / touch、writefile、rm

## 1. 本轮目标

本轮目标是在 MiniOS 现有只读文件表基础上，增加教学版 RAMFS，可在内存中创建、覆盖写入和删除小文本文件。

## 2. 为什么需要本任务

只读文件表只能验证“文件名 -> 内容”的读取链路，但还不能训练：

1. 创建文件
2. 修改文件内容
3. 删除文件
4. 更新文件元信息

RAMFS 刚好可以在不引入真实磁盘的前提下补上这部分最小语义。

## 3. RAMFS 当前定位

当前 RAMFS 是教学版内存文件系统：

1. 文件驻留在内存中
2. 系统重启后文件全部丢失
3. 当前只支持小文本文件
4. 当前不涉及真实磁盘、inode、block cache 或持久化

## 4. RAMFS 文件结构

当前每个 RAMFS 文件槽位最少记录：

1. `used`
2. `path`
3. `content`
4. `size`

文件数量和单文件大小都使用固定上限，不做动态扩容。

## 5. touch 语义

`touch <file>` 当前语义是：

1. 创建一个空 RAMFS 文件
2. 文件已存在时失败
3. 与内置只读文件同名时失败
4. RAMFS 表满时失败

## 6. writefile 语义

`writefile <file> <text>` 当前语义是：

1. 对已存在的 RAMFS 文件做覆盖写入
2. 当前不支持 append
3. 当前要求文件先存在，推荐先 `touch`
4. 内容超过单文件上限时失败
5. 内置只读文件不能写入

## 7. rm 语义

`rm <file>` 当前语义是：

1. 删除一个 RAMFS 文件
2. 删除后 `ls` 不再显示
3. 删除后 `cat/stat` 会失败
4. 内置只读文件不能删除

## 8. 与只读内置文件的关系

当前 MiniOS 同时暴露两类文件：

1. 内置只读文件
2. RAMFS 内存文件

区别是：

- 内置只读文件：只能读，不能删，不能改
- RAMFS 文件：可创建、可覆盖写入、可删除，但不持久化

## 9. 与 ls/cat/stat 的关系

当前这三类观察能力都已经能看到 RAMFS：

1. `ls` / `run ls`：列出 RAMFS 文件
2. `cat` / `run cat`：读取 RAMFS 文件内容
3. `run stat`：显示 RAMFS 文件的 `size/type`

其中 `run stat` 当前会把 RAMFS 文件显示为：

```text
Type: ramfs-file
```

## 10. 当前限制

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

## 11. 验证方式

本轮主要验证命令：

```text
touch /note.txt
writefile /note.txt hello
cat /note.txt
run cat /note.txt
run stat /note.txt
run ls
rm /note.txt
```

同时验证：

```text
writefile /readme.txt test
rm /readme.txt
```

确保内置只读文件仍然不可修改、不可删除。
