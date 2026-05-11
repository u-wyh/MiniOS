# MiniOS Phase2 用户程序表

## 1. 当前定位

当前 MiniOS Phase2 还没有真实文件系统。

因此：

```text
run hello
```

并不是像 Linux 一样去磁盘路径查找程序，而是走教学版最小链路：

```text
program name -> program_id -> 内置 ELF/blob -> exec -> process
```

Task50 的目标，就是把这条链路里的程序编号、程序名和内置镜像管理统一起来。

## 2. program_id 表

- `PROGRAM_EXEC_CHILD = 1`
- `PROGRAM_SHELL = 2`
- `PROGRAM_HELLO = 3`
- `PROGRAM_ECHO = 4`
- `PROGRAM_LOOP = 5`
- `PROGRAM_SLEEP_TEST = 6`
- `PROGRAM_INIT = 7`
- `PROGRAM_LOOP_EXIT = 8`
- `PROGRAM_INFO = 9`
- `PROGRAM_FORK = 10`
- `PROGRAM_FORKEXEC = 11`

其中：

- `PROGRAM_INVALID = 0` 表示非法编号
- `PROGRAM_COUNT` 只用于边界检查

## 3. 程序名映射

- `init -> PROGRAM_INIT`
- `shell -> PROGRAM_SHELL`
- `hello -> PROGRAM_HELLO`
- `echo -> PROGRAM_ECHO`
- `loop -> PROGRAM_LOOP`
- `loop_exit -> PROGRAM_LOOP_EXIT`
- `sleep_test -> PROGRAM_SLEEP_TEST`
- `execchild -> PROGRAM_EXEC_CHILD`
- `info -> PROGRAM_INFO`
- `fork -> PROGRAM_FORK`
- `forkexec -> PROGRAM_FORKEXEC`

其中 shell 默认直接暴露给用户的程序主要是：

- `hello`
- `echo`
- `loop`
- `loop_exit`
- `sleep_test`

兼容别名：

- `sleeptest -> sleep_test`
- `loopexit -> loop_exit`

## 4. exec 语义

当前教学版 `exec` 的最小语义是：

1. shell 先把程序名解析成 `program_id`
2. `SYS_EXEC_ARGS` 把 `program_id` 和最小 `argv` 交给内核
3. 内核通过统一用户程序表查到目标描述符
4. 描述符再关联到内置 ELF/blob
5. 内核把目标镜像装入当前进程地址空间

## 5. shell run/start 语义

- `run <program>`：前台执行，shell `fork` 子进程，子进程 `exec`，父进程 `waitpid`
- `start <program>`：后台执行，shell `fork` 子进程并 `exec`，父进程不等待

## 6. 用户程序参数传递

当前 MiniOS 还没有完整 `execve(path, argv, envp)` 和用户栈 `argc/argv` ABI，因此先采用教学版最小链路：

```text
shell token -> program_id -> SYS_EXEC_ARGS -> PCB 暂存 argv -> 用户程序通过 get_argc/get_arg 读取
```

例如：

```text
run echo hello minios
```

会被解释为：

- program name：`echo`
- argv[0]：`echo`
- argv[1]：`hello`
- argv[2]：`minios`

也就是说，当前实现保留了最小 Unix 风格语义：程序名会作为 `argv[0]` 传给新程序。

## 7. 参数限制

- 最大参数数量：`8`
- 单个参数最大长度：`31` 个可见字符，外加结尾 `'\0'`
- 参数过多：shell 会直接报错 `Too many args`，不会继续 `fork/exec`
- 参数过长：shell 会直接报错 `Arg too long`，内核 `process_copy_user_args()` 也会做兜底校验

## 8. echo 验证方式

- `run echo`：验证“无附加参数”路径，程序应安全输出空行并退出
- `run echo hello`：验证单参数传递
- `run echo hello minios phase2`：验证多参数与顺序保持

## 9. 当前限制

1. 暂不支持磁盘文件系统
2. 暂不支持 `PATH`
3. 暂不支持动态加载外部 ELF 文件
4. 暂不支持 `envp`
5. 暂不支持复杂引号、转义、管道和重定向
6. 当前 `argv` 保存在 PCB 暂存区里，还不是真实用户栈布局
7. 内置程序镜像仍由内核预先编译并嵌入
