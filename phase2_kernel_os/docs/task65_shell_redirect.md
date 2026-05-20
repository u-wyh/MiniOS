# Task65：Shell 输出重定向到 RAMFS / > 与 >> 雏形

## 1. 本轮目标

本轮目标是给 MiniOS Phase2 的 shell 增加最小版输出重定向语法，让：

```text
echo hello > /note.txt
echo world >> /note.txt
```

可以分别触发 RAMFS 覆盖写和 RAMFS 追加写。

## 2. 为什么需要本任务

在 Task62~64 中，MiniOS 已经有：

- RAMFS 文件创建
- 覆盖写入
- 追加写入

但这些能力主要还是通过 `writefile` / `append` 命令暴露。Task65 的意义，是让 shell 具备一层更接近真实系统的“输出到文件”语法。

## 3. 当前支持语法

当前只支持：

```text
echo text > /file
echo text >> /file
```

不支持：

- 任意用户程序 stdout 重定向
- 管道 + 重定向组合
- `dup2`
- 复杂引号解析

## 4. `>` 当前语义

`>` 对应 RAMFS 覆盖写。

行为：

1. shell 提取 `echo` 后、`>` 前的文本
2. 如果目标是已存在 RAMFS 文件，则覆盖旧内容
3. 如果目标文件不存在，则自动创建 RAMFS 文件再写入
4. 如果目标是内置只读文件，则失败

当前不自动添加换行，内容与 `writefile` 语义保持一致。

## 5. `>>` 当前语义

`>>` 对应 RAMFS 追加写。

行为：

1. shell 提取 `echo` 后、`>>` 前的文本
2. 目标文件必须已存在
3. 目标文件必须是 RAMFS 文件
4. 新文本追加到文件末尾
5. 超过 `MAX_RAMFS_FILE_SIZE` 时失败，且不破坏原内容

## 6. 与 `writefile` / `append` 的关系

可以把当前关系理解为：

```text
echo text > /file
    ~= writefile /file text
```

```text
echo text >> /file
    ~= append /file text
```

所以 Task65 不是新的文件系统后端，而是把已有的 RAMFS 写接口接到了 shell 语法层。

## 7. 只读文件保护

内置只读文件，例如：

- `/readme.txt`
- `/programs`
- `/help.txt`

都不能通过 `>` 或 `>>` 修改。

## 8. 当前限制

1. 暂不支持 stdin 重定向 `<`
2. 暂不支持 stderr 重定向 `2>`
3. 暂不支持 `2>&1`
4. 暂不支持 `dup/dup2`
5. 暂不支持通用用户程序 stdout 重定向
6. 暂不支持管道和重定向组合
7. 暂不支持后台任务重定向
8. 暂不支持复杂引号解析
9. 暂不支持真实磁盘和持久化

## 9. 验证方式

建议验证：

```text
touch /note.txt
echo hello > /note.txt
cat /note.txt
echo world >> /note.txt
cat /note.txt
run cat /note.txt
run stat /note.txt
echo test > /readme.txt
echo test >> /readme.txt
```
