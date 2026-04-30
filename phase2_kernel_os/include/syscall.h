#ifndef SYSCALL_H
#define SYSCALL_H

// 当前最小系统调用编号集合：write/exit/getpid/time
#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_GETPID 3
#define SYS_TIME 4

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

// 处理最小系统调用请求，当前只支持 SYS_WRITE
void syscall_handle(struct interrupt_frame* frame);
// 查询当前系统调用是否请求“测试收口停机”
int syscall_should_halt(void);
// 清理上一轮系统调用留下的 halt 请求状态
void syscall_clear_halt(void);

#endif
