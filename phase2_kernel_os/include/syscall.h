// syscall.h：定义最小系统调用号、用户态中断现场和 syscall 分发接口
#ifndef SYSCALL_H
#define SYSCALL_H

// 基础输出
#define SYS_WRITE 1

// 进程生命周期
#define SYS_EXIT 2
#define SYS_GETPID 3
#define SYS_TIME 4
#define SYS_FORK 5
#define SYS_WAITPID 6
#define SYS_EXEC 7

// 输入 / 参数读取
#define SYS_READ_CHAR 8
#define SYS_GET_ARGC 9
#define SYS_GET_ARG 10
#define SYS_EXEC_ARGS 11

// 进程观察 / 控制
#define SYS_PS 12
#define SYS_KILL 13
#define SYS_WAIT_ANY 14
#define SYS_SET_BACKGROUND 18

// 调度 / 时间
#define SYS_YIELD 15
#define SYS_SLEEP 16
// 教学调试接口：按 pid 让目标进程睡眠，当前主要给 shell 调试命令复用。
#define SYS_SLEEP_PID 17
#define SYS_GET_TICKS 19

// 界面辅助
#define SYS_CLEAR_SCREEN 20

// 只读文件 fd 雏形
#define SYS_OPEN 21
#define SYS_READ 22
#define SYS_CLOSE 23

// 只读文件列表查询
#define SYS_FILE_COUNT 24
#define SYS_FILE_INFO 25
#define SYS_STAT 26

// RAMFS 教学版写接口
#define SYS_TOUCH 27
#define SYS_WRITEFILE 28
#define SYS_RM 29
// RAMFS fd 写入雏形：避免破坏已有 stdout SYS_WRITE ABI。
#define SYS_OPEN_WRITE 30
#define SYS_FD_WRITE 31
// RAMFS 教学版追加写入接口：按路径把文本追加到文件末尾，不实现完整 POSIX O_APPEND。
#define SYS_APPEND_FILE 32
// 教学版 stdout 重定向配置：仅作用于当前进程，不实现完整 dup2/fd 复制语义。
#define SYS_SET_STDOUT_REDIRECT 33
// 教学版 stdin 重定向配置：仅作用于当前进程，不实现完整 dup2/fd 复制语义。
#define SYS_SET_STDIN_REDIRECT 34
// 教学版单管道：清空全局 pipe buffer。
#define SYS_PIPE_RESET 35
// 教学版单管道：把指定 pid 的 stdout 改为写入 pipe buffer。
#define SYS_SET_STDOUT_PIPE 36
// 教学版单管道：把指定 pid 的 stdin 改为从 pipe buffer 读取。
#define SYS_SET_STDIN_PIPE 37

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
// 查询当前 syscall 是否希望先退回内核 idle/hlt 路径
int syscall_should_idle(void);
// 清理一次性的 idle 请求标志
void syscall_clear_idle(void);
// 登记下一次 syscall 返回时应切换到的新用户态现场
void syscall_set_resume_frame(struct interrupt_frame* frame);
// 取出待切换的用户态现场；返回后内部会清空该记录
struct interrupt_frame* syscall_take_resume_frame(void);

#endif
