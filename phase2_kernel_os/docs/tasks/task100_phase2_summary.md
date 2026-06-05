# Task100：Phase2 fd / pipe / process 总结文档

## 1. 任务目标

本轮不继续新增内核功能，而是把 Phase2 已经完成的主线整理成一份系统总结文档，帮助后续复习、答辩和面试讲述。

重点回答这些问题：

1. fd table 是什么
2. stdin/stdout 怎么工作
3. redirect 怎么改变数据流
4. pipe fd 和 pipe object 是什么关系
5. fork / dup2 / exec 为什么能组合成 pipeline
6. argv 和数据流有什么区别
7. 当前系统和 Linux/UNIX 还差什么

## 2. 为什么进入收尾总结

到 Task99 为止，Phase2 主线已经基本贯通：

1. 用户程序
2. RAMFS
3. fd table
4. redirect
5. pipe
6. pipe object table
7. fork
8. dup2
9. exec
10. argv
11. mini_pipeline
12. shell 多级管道

这时继续盲目加功能，收益已经不如把已有能力讲清楚。

## 3. 本轮新增文档

本轮新增：

1. `docs/phase2_summary.md`
2. `docs/tasks/task100_phase2_summary.md`

## 4. 本轮更新文档

本轮同步更新：

1. `README.md`
2. `docs/phase2.md`
3. `docs/fd.md`
4. `docs/pipe.md`
5. `docs/process.md`
6. `docs/shell.md`
7. `docs/syscall.md`
8. `docs/user_programs.md`

## 5. 本轮整理出的主线

Phase2 当前最适合用这一条线来理解：

```text
用户输入 shell 命令
  -> shell 解析 argv / redirect / pipe
    -> fd table / pipe object 搭线
      -> fork 子进程
        -> dup2 改 stdin/stdout 指向
          -> exec 替换目标程序
            -> 用户程序 read(0) / write(1)
```

这条主线的关键强调点是：

1. `argv` 是控制参数
2. stdin/stdout 才是数据流
3. `dup2` 改的是 fd 指向
4. `exec` 必须保留 fd table

## 6. 当前功能边界

本轮文档明确保留这些边界说明：

1. 当前不是完整 POSIX
2. pipe close 语义仍然是教学版
3. fd 引用计数仍然简化
4. shell parser 仍然不是完整 UNIX shell
5. 仍然没有环境变量、信号、进程组、job control、TTY 完整抽象
6. 仍然没有真实磁盘文件系统和完整 ELF 动态加载

## 7. 后续方向

当前建议的后续工作是：

1. Task101：Phase2 回归测试清单
2. Task102：Phase2 收尾冻结 / phase2-complete 节点整理

也就是说，Task100 是“把系统讲清楚”，Task101/Task102 更偏“把阶段收尾做扎实”。
