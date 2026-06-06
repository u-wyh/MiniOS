# Task75：用户态 head 程序 / 读取前 N 行

## 1. 任务目标

本轮目标是新增一个最小用户态 `head` 程序，用于验证 stdin / pipe / stdout redirect 下的数据截断。

## 2. 为什么新增 head

当前 MiniOS Phase2 已经有：

1. `cat`：复制数据
2. `wc`：统计数据
3. `grep`：过滤数据

新增 `head` 后，可以继续验证另一种常见文本处理模式：

```text
输入数据 -> head -> 只保留前 N 行
```

这说明当前 shell、stdin/stdout redirect 和教学版 pipe 已经足够支撑“按行截断”类用户态工具。

## 3. 修改文件

本轮主要修改：

1. `include/user_program.h`
2. `include/fs.h`
3. `kernel/fs_parts/embedded_and_tables.inc`
4. `kernel/user_program_sources/head_elf_source.c`
5. `readme.md`
6. `docs/phase2.md`
7. `docs/user_programs.md`

并新增：

1. `kernel/user_program_blobs/head_elf.inc`
2. `docs/tasks/task75_user_head.md`

## 4. 实现思路

当前 `head` 保持教学版最小实现：

1. 默认输出前 10 行
2. 支持 `-n N`
3. 统一从 `fd=0` 读取 stdin
4. 按小 buffer 循环 `sys_read(0, ...)`
5. 扫描 `'\n'` 统计行数
6. 达到目标行数后立即停止输出并退出

## 5. 核心语义

当前支持：

```text
run head
run head < /readme.txt
run head -n 3 < /readme.txt
run cat /readme.txt | run head
run cat < /readme.txt | run head -n 3 > /head.txt
```

语义说明：

1. `head` 默认输出前 10 行
2. `head -n N` 输出前 N 行
3. `head -n 0` 正常不输出并退出
4. 参数错误时输出简单 Usage，不 panic
5. `head` 不从 argv 文件路径读文件，统一从 stdin 读取

## 6. 验证命令

本轮建议验证：

```text
run head < /readme.txt
run head -n 3 < /readme.txt
run head -n 0 < /readme.txt
run head -n abc < /readme.txt
run cat /readme.txt | run head
run cat /readme.txt | run head -n 3
run head -n 3 < /readme.txt > /head.txt
cat /head.txt
run cat < /readme.txt | run head -n 3 > /head2.txt
cat /head2.txt
```

## 7. 当前限制

1. 暂不支持多个文件参数
2. 暂不支持完整 GNU `head` 参数
3. 暂不支持 `run head /file`
4. 暂无交互式 tty stdin
5. 当前仍基于教学版 pipe，而不是真正 UNIX pipe / pipe fd / `dup2`

## 8. 后续方向

后续可以继续推进：

1. `tail` 等文本截断工具
2. 更完整的 `head` 参数语义
3. pipe buffer 容量与错误处理增强
4. 更接近真实 UNIX 的 fd / `dup2` / pipe 模型
