// exec.h：声明按文件名发起用户程序执行请求的最小接口
#ifndef EXEC_H
#define EXEC_H

// 按文件名执行用户程序：内部通过 process_create/process_run 进入进程模型
void exec(const char* name);
// 登记一次待执行的文件请求，供内核主循环在中断上下文外执行
void exec_request(const char* name);
// 查询当前是否存在待执行请求
int exec_has_pending_request(void);
// 执行并清理当前待处理请求
void exec_run_pending(void);

#endif
