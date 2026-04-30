#include "task.h"
#include "vga.h"

// 两个最小任务控制块：当前阶段只做 A/B 任务轮换
static task_t tasks[2];
// 记录当前活跃任务编号，-1 表示还未切入任何任务
static int current_task = -1;
// 保存 Shell 所在调度上下文的栈指针，便于任务让回控制权
static unsigned int scheduler_esp = 0;

// 为两个任务分别分配独立 4KB 栈空间
static unsigned char stack_a[4096];
static unsigned char stack_b[4096];

// 汇编上下文切换：保存旧栈并切换到新栈，然后 ret 到新任务
extern void context_switch(unsigned int* old_esp, unsigned int new_esp);

// 任务让出 CPU：把当前任务现场保存回 TCB，并返回到 Shell 上下文
static void task_yield(void) {
    context_switch(&tasks[current_task].esp, scheduler_esp);
}

// 任务 A：每次被手动切入时输出一个字符 A，然后让回 Shell
static void task_a(void) {
    for (;;) {
        print_char('A');
        task_yield();
    }
}

// 任务 B：每次被手动切入时输出一个字符 B，然后让回 Shell
static void task_b(void) {
    for (;;) {
        print_char('B');
        task_yield();
    }
}

// 构造任务第一次启动所需的最小栈帧，使 ret 能进入任务函数
static unsigned int build_initial_esp(unsigned char* stack_base, void (*entry)(void)) {
    unsigned int* sp = (unsigned int*)(stack_base + 4096);

    // ret 将跳转到任务入口，因此先压入入口地址
    *--sp = (unsigned int)entry;
    // 下面 8 个槽位对应 context_switch 中 popad 恢复的寄存器
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

// 手动在任务 A 和任务 B 之间切换：每次只让目标任务执行一步
void switch_task(void) {
    if (current_task == 0) {
        current_task = 1;
    } else {
        current_task = 0;
    }

    context_switch(&scheduler_esp, tasks[current_task].esp);
}
