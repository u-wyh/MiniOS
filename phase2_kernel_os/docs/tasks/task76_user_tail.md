# Task76：用户态 tail 程序 / 简化版尾部输出

## 1. 任务目标

本轮目标是新增一个最小用户态 `tail` 程序，用来验证当前 MiniOS 的数据流已经可以支撑“先完整读取，再决定输出哪一段”的文本工具。

目标链路：

```text
stdin / pipe / file redirect -> tail -> 最后 N 行 -> stdout / redirect file
```

## 2. 为什么新增 tail

`head` 可以边读边停，因为它只需要前 N 行。

`tail` 不一样：它必须先看到输入尾部，才能知道“最后 N 行”到底从哪里开始。

因此，`tail` 是当前阶段验证教学版 `stdin / pipe / stdout redirect` 的一个更强测试点：

1. 读取必须能从 `fd=0` 统一获取。
2. 用户程序必须可以自己缓存一段输入。
3. 处理逻辑必须发生在用户态，而不是继续往内核里塞特殊规则。

## 3. 修改文件

本轮主要修改：

1. `include/user_program.h`
2. `include/fs.h`
3. `kernel/fs_parts/embedded_and_tables.inc`
4. `kernel/user_program_sources/tail_elf_source.c`
5. `kernel/user_program_blobs/tail_elf.inc`
6. `readme.md`
7. `docs/phase2.md`
8. `docs/user_programs.md`
9. `docs/tasks/task76_user_tail.md`

## 4. 实现思路

当前 `tail` 采用教学版最小方案：

1. 统一通过 `SYS_READ(fd=0)` 从 stdin 读取。
2. 使用固定大小窗口缓存最近一段输入。
3. 如果输入超过窗口容量，就丢弃更早的数据，只保留最后窗口内容。
4. 读取结束后，从后往前查找换行符。
5. 定位到最后 N 行的起始位置，再整体输出。

这样可以最小复用现有 `stdin / pipe / redirect` 路径，不引入新的内核机制。

## 5. 核心语义

当前 `tail` 支持：

```text
run tail
run tail < /readme.txt
run tail -n 3 < /readme.txt
run cat /readme.txt | run tail
run cat /readme.txt | run tail -n 3
run tail -n 3 < /readme.txt > /tail.txt
run cat < /readme.txt | run tail -n 3 > /tail2.txt
```

当前语义：

1. 默认输出最后 10 行。
2. `tail -n N` 输出最后 N 行。
3. `tail -n 0` 正常不输出并退出。
4. `tail` 不从 argv 文件路径读文件，统一从 stdin 读取。
5. 文件输入重定向和 pipe 输入都走 `fd=0`。
6. 输出统一通过 `SYS_WRITE(...)`，因此可落屏幕，也可重定向到 RAMFS 文件。

## 6. 验证命令

建议至少验证：

```text
run tail < /readme.txt
run tail -n 3 < /readme.txt
run tail -n 0 < /readme.txt
run tail -n abc < /readme.txt
run cat /readme.txt | run tail
run cat /readme.txt | run tail -n 3
run tail -n 3 < /readme.txt > /tail.txt
cat /tail.txt
run cat < /readme.txt | run tail -n 3 > /tail2.txt
cat /tail2.txt
run head -n 3 < /readme.txt
run tail -n 3 < /readme.txt
```

## 7. 当前限制

当前 `tail` 仍是教学版最小实现：

1. 不支持多个文件参数。
2. 不支持完整 GNU `tail` 参数。
3. 不支持 `-f`。
4. 不支持 `run tail /file` 这种 argv 文件模式。
5. 使用固定缓冲区，不做大文件优化。
6. 当输入超过固定窗口时，只保证“保留窗口内最后 N 行”。
7. 仍基于当前教学版单管道，不支持真正 UNIX pipe / pipe fd / `dup2`。

## 8. 后续方向

后续可以继续推进：

1. `sort` 等需要更复杂缓存策略的用户态文本工具。
2. pipe buffer 容量限制和错误提示整理。
3. 更完整的 `head/tail` 参数语义。
4. 更真实的文件系统与 `exec` 装载路径。
