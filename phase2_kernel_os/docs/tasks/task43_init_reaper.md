# Task43：init reaper 循环雏形

## 1. reparent 和 reap 的区别是什么？

- reparent：把孤儿进程的 `parent_pid` 改到 init，不改变运行状态。
- reap：对 `ZOMBIE` 子进程做最终回收，释放资源并清理 PCB。

## 2. 为什么 ZOMBIE 必须由父进程 wait 回收？

子进程退出后仍保留退出信息，父进程需要通过 wait 路径读取并触发最终释放。  
如果不 wait，进程槽位会长期占用。

## 3. init 为什么适合作为孤儿进程的 reaper？

当前 MiniOS 里 init 是根用户进程。  
孤儿进程 reparent 到 init 后，可以统一由 init 执行周期性回收。

## 4. wait_any 的最小语义是什么？

- `>0`：成功回收一个子进程并返回 pid
- `0`：当前没有可回收 zombie
- `<0`：错误

当前是非阻塞接口，不等待子进程退出。

## 5. 为什么本轮采用非阻塞 wait_any？

为了保持教学版最小复杂度，不引入 wait 队列和阻塞调度细节。  
init 通过循环周期性调用 wait_any 就能形成回收闭环。

## 6. init reaper 为什么不能误回收非 init 子进程？

wait_any 只回收“当前进程名下（parent_pid 匹配）且 state=ZOMBIE”的子进程，  
不会动 READY/RUNNING 子进程，也不会回收其他父进程的子进程。

## 7. 当前实现和真实 Linux init / wait / SIGCHLD 的差距

- 暂无 SIGCHLD
- 暂无完整 wait(-1) 阻塞语义
- 暂无完整 init 服务管理
- 暂无进程组/session/终端控制

## 8. 后续要支持完整 wait(-1) 还需要补什么？

- 阻塞 wait 队列
- 子进程状态变化通知机制
- 更完整的并发与父子同步处理
