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

## 6. 当前限制

1. 暂不支持磁盘文件系统
2. 暂不支持 `PATH`
3. 暂不支持动态加载外部 ELF 文件
4. 暂不支持 `envp`
5. 内置程序镜像仍由内核预先编译并嵌入
