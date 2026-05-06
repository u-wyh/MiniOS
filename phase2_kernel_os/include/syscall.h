// syscall.h：定义最小系统调用号、用户态中断现场和 syscall 分发接口
#ifndef SYSCALL_H
#define SYSCALL_H

// 当前最小系统调用编号集合：write/exit/getpid/time/fork/waitpid/exec/read_char/get_argc/get_arg/exec_args/ps
#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_GETPID 3
#define SYS_TIME 4
#define SYS_FORK 5
#define SYS_WAITPID 6
#define SYS_EXEC 7
#define SYS_READ_CHAR 8
#define SYS_GET_ARGC 9
#define SYS_GET_ARG 10
#define SYS_EXEC_ARGS 11
#define SYS_PS 12
#define SYS_KILL 13
#define SYS_WAIT_ANY 14
#define SYS_YIELD 15
#define SYS_SLEEP 16
#define SYS_SLEEP_PID 17
#define SYS_SET_BACKGROUND 18

// int 0x80 进入内核后，栈上会按 pusha + CPU 自动压栈的顺序保存现场
struct interrupt_frame {
    unsigned int edi;
    unsigned int esi;
    unsigned int ebp;
    unsigned int esp_placeholder;
    unsigned int ebx;
    unsigned int edx;
    unsigned int ecx;
    unsigned int eax;
    unsigned int eip;
    unsigned int cs;
    unsigned int eflags;
    unsigned int user_esp;
    unsigned int user_ss;
};

// 处理最小系统调用请求，当前支持 write/exit/getpid/time
void syscall_handle(struct interrupt_frame* frame);
// 查询当前系统调用是否请求离开用户态回到内核控制台
int syscall_should_halt(void);
// 清理上一轮系统调用留下的退出请求状态
void syscall_clear_halt(void);
// 登记下一次 syscall 返回时应切换到的新用户态现场
void syscall_set_resume_frame(struct interrupt_frame* frame);
// 取出待切换的用户态现场；返回后内部会清空该记录
struct interrupt_frame* syscall_take_resume_frame(void);

#endif
