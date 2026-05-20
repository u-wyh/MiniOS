# Task64：RAMFS append 追加写入 / 用户态 append 程序

## 1. 本轮目标

本轮目标是在已有 RAMFS 覆盖写基础上，补充最小教学版 append 追加写入语义。

## 2. 为什么需要本任务

覆盖写和追加写是两种不同的文件写入语义：

- `writefile`：从文件开头重新写，旧内容被替换
- `append`：从文件末尾继续写，旧内容保留

append 是后续 `>>` 重定向、日志文件、append mode fd 的基础。

## 3. append 当前语义

当前 append 采用教学版最小语义：

1. 只支持 RAMFS 文件
2. 不自动创建文件
3. 不自动添加空格
4. 不自动添加换行
5. 从当前 `size` 位置写入
6. 成功后更新 `size`
7. 超过 `MAX_RAMFS_FILE_SIZE` 时失败
8. 失败时不破坏原内容

示例：

```text
writefile /note.txt hello
append /note.txt world
```

结果：

```text
helloworld
```

## 4. append 与 writefile 的区别

`writefile`：

- 覆盖写入
- 最终内容等于新文本

`append`：

- 追加写入
- 最终内容等于旧内容 + 新文本

## 5. shell append

当前 shell 内建命令支持：

```text
append /note.txt world
```

它直接调用内核 RAMFS append 接口。

## 6. 用户态 append

当前用户态程序支持：

```text
run append /note.txt world
```

它通过 syscall 请求内核执行 append，而不是直接访问 RAMFS 文件表。

## 7. 只读文件保护

内置只读文件（如 `/readme.txt`）不能 append：

```text
append /readme.txt test
run append /readme.txt test
```

都会失败，原文件内容保持不变。

## 8. 当前限制

1. 暂不支持真实磁盘
2. 暂不支持持久化
3. 暂不支持完整 POSIX `O_APPEND`
4. 暂不支持并发原子追加
5. 暂不支持文件锁
6. 暂不支持 `>>` 重定向
7. 暂不支持 pipe
8. 暂不支持 dup/dup2
9. 暂不支持复杂 shell 引号解析
10. 后续可以扩展 append mode fd、`>>` 重定向、日志文件

## 9. 验证方式

```text
touch /note.txt
writefile /note.txt hello
append /note.txt world
run append /note.txt !
cat /note.txt
run stat /note.txt
run append /readme.txt test
```
