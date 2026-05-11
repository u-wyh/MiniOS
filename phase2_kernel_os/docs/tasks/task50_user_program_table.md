# Task50：用户程序表 / program_id 整理

## 1. 本轮目标

把 MiniOS Phase2 当前内置用户程序的编号、名称和镜像查询入口统一起来，避免 shell、process、exec 各自维护多份重复映射。

## 2. 为什么需要本任务

当前系统还没有真实文件系统，所以：

```text
run hello
```

本质上不是“路径查找 + 磁盘加载”，而是：

```text
program name -> program_id -> 内置 ELF/blob -> exec -> process
```

如果这条链路里的映射散落在多个文件中，后续新增用户程序或修改程序名时很容易出错。

## 3. 本轮修改内容

- 新增统一的 `user_program.h` / `user_program.c`
- 定义统一 `program_id`
- 定义统一用户程序描述表
- 提供 `user_program_get_by_id()` / `user_program_find_by_name()` 等查询接口
- 让 shell 的 `run/start` 复用统一程序表
- 让内核 exec 路径复用统一程序表

## 4. 当前用户程序表

当前统一程序表包含：

- `init`
- `shell`
- `hello`
- `echo`
- `loop`
- `loop_exit`
- `sleep_test`
- `execchild`
- `info`
- `fork`
- `forkexec`

## 5. 验证方式

- `run hello`
- `run echo hello minios`
- `run loopexit`
- `start loop`
- `run sleeptest`

这些命令都应能通过统一 `program_id` 表找到目标程序。

## 6. 当前限制

1. 暂不支持真实文件系统
2. 暂不支持 `PATH`
3. 暂不支持外部 ELF 动态加载
4. 暂不支持 `envp`
5. 当前仍是教学版最小 exec 语义

## 7. 后续任务

Task50 之后，后续更自然的工作是整理用户程序参数传递，让 shell `run/start` 到 exec 的 `argc/argv` 语义更清晰。
