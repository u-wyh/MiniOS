#include "task.h"
#include "vga.h"

// 两个最小任务控制块：当前阶段只做 A/B 任务轮换
static task_t tasks[2];
// 每个任务独立维护一个运行代号，只有代号变化时才输出一个新字符
static volatile unsigned int task_run_tokens[2] = {1, 0};

// 为两个任务分别分配独立 4KB 栈空间
static unsigned char stack_a[4096];
static unsigned char stack_b[4096];

// 汇编入口：把控制流直接切到准备好的任务中断现场，并通过 iret 进入任务
extern void task_enter(unsigned int new_esp);

// 任务 A：每次获得新的时间片时输出一个字符 A，然后 hlt 等待下一次中断
static void task_a(void) {
    unsigned int last_seen_token = 0;

    for (;;) {
        if (task_run_tokens[0] != last_seen_token) {
            last_seen_token = task_run_tokens[0];
            print_char('A');
        }

        __asm__ __volatile__("hlt");
    }
}

// 任务 B：每次获得新的时间片时输出一个字符 B，然后 hlt 等待下一次中断
static void task_b(void) {
    unsigned int last_seen_token = 0;

    for (;;) {
        if (task_run_tokens[1] != last_seen_token) {
            last_seen_token = task_run_tokens[1];
            print_char('B');
        }

        __asm__ __volatile__("hlt");
    }
}

// 构造任务第一次启动所需的最小中断现场，使 popad + iret 能直接进入任务函数
static unsigned int build_initial_esp(unsigned char* stack_base, void (*entry)(void)) {
    unsigned int* sp = (unsigned int*)(stack_base + 4096);

    // 先伪造 iret 需要恢复的 EFLAGS / CS / EIP，使任务像中断返回一样启动
    *--sp = 0x00000202; // eflags：保留 IF=1，让任务中的 hlt 能被时钟中断唤醒
    *--sp = 0x00000008; // cs：沿用当前 GDT 中的内核代码段选择子
    *--sp = (unsigned int)entry; // eip：任务入口函数地址
    // 再补上 popad 要恢复的 8 个通用寄存器槽位
    *--sp = 0; // eax
    *--sp = 0; // ecx
    *--sp = 0; // edx
    *--sp = 0; // ebx
    *--sp = 0; // esp 占位
    *--sp = 0; // ebp
    *--sp = 0; // esi
    *--sp = 0; // edi

    return (unsigned int)sp;
}

// 初始化两个任务的独立栈和首次运行入口
void task_init(void) {
    tasks[0].esp = build_initial_esp(stack_a, task_a);
    tasks[1].esp = build_initial_esp(stack_b, task_b);
}

// 从内核主流程启动第一个任务，后续切换改由 PIT 中断驱动
void task_start_first(int task_index) {
    task_enter(tasks[task_index].esp);
}

// 读取指定任务保存的栈指针，供调度器切换到目标任务
unsigned int task_get_esp(int task_index) {
    return tasks[task_index].esp;
}

// 把被抢占任务的最新 ESP 写回 TCB，便于下次恢复
void task_set_esp(int task_index, unsigned int esp) {
    tasks[task_index].esp = esp;
}

// 返回当前演示任务数量，方便调度器做最小 round-robin
int task_count(void) {
    return 2;
}

// 标记任务进入新的时间片，让任务函数只在重新被调度时输出一个字符
void task_mark_scheduled(int task_index) {
    task_run_tokens[task_index]++;
}

// 返回任务当前运行代号，供其他模块观察调度是否生效
unsigned int task_get_run_token(int task_index) {
    return task_run_tokens[task_index];
}
