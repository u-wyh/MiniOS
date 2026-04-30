#ifndef USER_H
#define USER_H

// 初始化最小用户空间：建立用户代码页、用户栈页，并准备一次 Ring3 测试
void user_space_init(void);
// 由 shell 命令发起一次用户态测试请求，真正切换由内核主循环完成
void user_request_enter(void);
// 内核主循环轮询是否有待执行的用户态测试请求
int user_has_pending_request(void);
// 真正执行一次用户态切换，使用已经规划好的用户虚拟地址布局
void user_enter_mode(void);

#endif
