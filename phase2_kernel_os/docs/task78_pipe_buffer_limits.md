# Task78：pipe buffer 容量限制与错误处理整理

## 1. 任务目标

本轮目标不是实现真正 UNIX pipe，而是把当前教学版顺序 pipe 的边界行为整理清楚：

1. 明确固定容量
2. 明确写满时的行为
3. 明确空读 / EOF 语义
4. 明确每次命令前后的初始化与清理
5. 避免连续 pipe 命令复用旧数据
6. 保证错误路径不 panic

## 2. 为什么现在整理 pipe buffer

在 Task69~Task77 之后，MiniOS Phase2 已经有了：

1. `stdin` 重定向
2. `stdout` 重定向
3. 教学版单管道
4. `cat / wc / grep / head / tail / sort`

这意味着 pipe 已经不再只是一个演示功能，而是数据流主链路的一部分。

如果不整理边界行为，就容易出现：

1. buffer 越界
2. 写满后返回路径混乱
3. 多次 pipe 命令互相污染
4. pipe 和 redirect 组合后状态不一致

所以本轮的重点是“稳定性收口”。

## 3. 修改文件

本轮最小改动包括：

1. `include/process.h`
2. `kernel/process_parts/core_helpers.inc`
3. `kernel/process_parts/fd_and_input.inc`
4. `kernel/process_parts/redirect_pipe.inc`
5. `readme.md`
6. `docs/phase2.md`
7. `docs/pipe.md`
8. `docs/task78_pipe_buffer_limits.md`

## 4. 实现思路

当前采用最小改动策略：

1. 继续保留现有教学版顺序 pipe 模型
2. 不引入 pipe fd
3. 不引入阻塞读写
4. 不改 shell parser 主体
5. 在现有 `process_pipe_buffer` 上补清楚状态语义
6. 在写入路径上补容量检查
7. 在读取路径上补最小 EOF 语义说明
8. 保持每次 pipe 前后清空状态

## 5. 核心语义

本轮整理后，pipe 的核心语义是：

1. 容量固定为 `512` 字节
2. 左侧写 pipe 时不会越界
3. 写满时会尽量写满剩余空间
4. 写满时只提示一次：

```text
pipe: buffer full
```

5. 后续继续写返回 `0`
6. 右侧读取到 `read_offset >= size` 时返回 `0`
7. 空 pipe 读取也返回 `0`
8. 每次 pipe 命令前后都会 reset 状态

## 6. 验证命令

建议验证命令包括：

```text
run cat /readme.txt | run cat
run cat /readme.txt | run wc
run cat /readme.txt | run grep MiniOS
run cat /readme.txt | run head -n 3
run cat /readme.txt | run tail -n 3
run cat /readme.txt | run sort
run cat /readme.txt | run grep MiniOS > /grep.txt
run cat < /readme.txt | run wc
run cat < /readme.txt | run sort > /sorted.txt
```

连续执行也要检查：

```text
run cat /readme.txt | run head -n 1
run cat /readme.txt | run tail -n 1
run cat /readme.txt | run grep MiniOS
```

## 7. 当前限制

当前教学版 pipe 仍然不支持：

1. 真正 UNIX pipe
2. pipe fd
3. `dup2`
4. 阻塞读写
5. 并发执行
6. 多级管道
7. 动态扩容

## 8. 后续方向

后续可以继续沿三个方向推进：

1. Task79：真正 pipe fd 雏形
2. shell parser 与错误提示整理
3. Phase2 数据流演示脚本与讲解材料
