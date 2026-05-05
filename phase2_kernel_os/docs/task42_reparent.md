# Task42：孤儿进程 reparent 到 init

## 1. 什么是孤儿进程？

父进程退出后，仍然存活的子进程就是孤儿进程。

## 2. 为什么 shell 退出后后台任务会变成孤儿进程？

`start <program>` 启动的是 shell 子进程。  
如果 shell 提前退出，后台子进程还在运行，就会失去父进程。

## 3. 为什么需要把孤儿进程交给 init？

在教学版 MiniOS 里，init 是根用户进程。  
把孤儿进程交给 init，后续才有稳定的父进程可用于 `waitpid` 回收。

## 4. reparent 具体修改了什么？

父进程退出时扫描进程表：  
把 `parent_pid == 退出进程 pid` 的有效子进程改为 `parent_pid = init_pid`。

## 5. reparent 为什么不等于 kill？

reparent 只改父子关系，不会终止子进程，也不会修改其退出码。

## 6. reparent 为什么不释放资源？

资源释放仍由“子进程退出 + 父进程 waitpid”完成。  
reparent 不是回收动作，只是关系迁移。

## 7. init 在当前 MiniOS 中承担什么角色？

- 系统首个用户进程
- shell 的父进程
- 孤儿进程接管目标（最小 reaper）

## 8. 当前实现和真实 Linux reparent 还有哪些差距？

当前是教学版最小实现，暂不包含：

- 进程组与 session
- 终端控制
- 完整 signal 系统
- 完整 job control（jobs/fg/bg）
