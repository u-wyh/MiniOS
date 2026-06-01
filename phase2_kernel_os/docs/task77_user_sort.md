# Task77：用户态 sort 程序 / 小输入行排序

## 1. 任务目标

本轮目标是在 MiniOS Phase2 里新增一个最小用户态 `sort` 程序，用来验证：

```text
stdin / pipe / 文件输入
  -> 用户态缓存
    -> 按行切分
      -> 行排序
        -> stdout / 重定向文件
```

这里的重点不是实现完整 GNU `sort`，而是继续验证当前教学版数据流已经可以支撑“先读完、再处理、再输出”的用户态文本工具。

## 2. 为什么新增 sort

前面的用户态文本工具链已经有：

1. `cat`：复制数据
2. `wc`：统计数据
3. `grep`：按行过滤
4. `head`：截取前部
5. `tail`：截取尾部

`sort` 再往前走一步，因为它要求：

1. 先把 stdin 全部读入用户态
2. 在用户态内部切分出多行
3. 对多行做比较和交换
4. 再按新的顺序输出

这能验证当前 MiniOS 的 stdin / pipe / stdout redirect 已经不只是“把数据传过去”，而是能支撑更复杂的文本处理。

## 3. 修改文件

本轮最小改动包括：

1. `include/user_program.h`
2. `include/fs.h`
3. `kernel/fs_parts/embedded_and_tables.inc`
4. `kernel/user_program_sources/sort_elf_source.c`
5. `kernel/user_program_blobs/sort_elf.inc`
6. `readme.md`
7. `docs/phase2.md`
8. `docs/user_programs.md`
9. `docs/task77_user_sort.md`

## 4. 实现思路

当前采用教学版最简单方案：

1. 从 `fd=0` 循环读取 stdin
2. 把全部输入放进固定大小缓冲区
3. 按 `\n` 切成若干行
4. 为每一行记录起始位置、长度和是否带换行
5. 使用简单冒泡排序按字典序升序排列
6. 再按排序结果顺序输出每一行

为了保持最小实现，本轮不引入动态内存，也不实现大文件外部排序。

## 5. 核心语义

当前 `sort` 支持：

```text
run sort
run sort < /readme.txt
run cat /readme.txt | run sort
run sort < /readme.txt > /sorted.txt
run cat < /readme.txt | run sort > /sorted2.txt
```

当前语义：

1. `sort` 统一从 stdin 读取
2. 输入按 `\n` 切成若干行
3. 每一行按字节字典序升序排序
4. 前缀相同时较短行排在前面
5. 保留原始换行；最后一行即使没有换行也会输出
6. 空输入时正常退出

## 6. 验证命令

建议最小验证命令：

```text
run sort < /readme.txt
run cat /readme.txt | run sort
run sort < /readme.txt > /sorted.txt
cat /sorted.txt
run cat < /readme.txt | run sort > /sorted2.txt
cat /sorted2.txt
```

构造三行排序的示例：

```text
touch /sort.txt
writefile /sort.txt banana
append /sort.txt apple
append /sort.txt cat
run sort < /sort.txt
```

如果当前 `writefile / append` 的换行行为和预期略有差异，可以按系统实际输入方式调整样例，但需要保证验证到三行重排。

## 7. 当前限制

当前 `sort` 仍是教学版：

1. 不支持多个文件参数
2. 不支持完整 GNU `sort` 参数
3. 不支持 `-r`
4. 不支持 `-n`
5. 不支持去重
6. 不支持 locale 排序
7. 使用固定缓冲区
8. 使用固定最大行数
9. 不支持大文件外部排序
10. 仍然依赖当前教学版 pipe，而不是完整 UNIX pipe fd

## 8. 后续方向

后续可以继续往三个方向推进：

1. 整理 pipe buffer 的错误处理和容量限制
2. 推进真正的 pipe fd / `dup2` 雏形
3. 补一组 Phase2 数据流演示脚本与讲解材料
