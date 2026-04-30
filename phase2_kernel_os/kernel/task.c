#include "task.h"
#include "vga.h"

// 两个最小任务控制块：TCB 只保存 esp，因为完整现场已经留在各自任务栈里
static task_t tasks[2];
// 每个任务独立维护一个运行代号，只有代号变化时才输出一个新字符
static volatile unsigned int task_run_tokens[2] = {1, 0};

// 为两个任务分别分配独立 4KB 栈空间
static unsigned char stack_a[4096];
static unsigned char stack_b[4096];

// 汇编入口：把控制流直接切到准备好的任务中断现场，并通过 iret 进入任务
extern void task_enter(unsigned int new_esp);

// 这个结构精确描述“任务被恢复前”栈里的保存现场布局
// 当前自动调度路径要求：popad 先恢复通用寄存器，再由 iretd 恢复 eip/cs/eflags
struct task_stack_frame {
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
};

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

// 构造任务第一次启动所需的完整现场，使 popad + iretd 能正确恢复寄存器并进入任务
static unsigned int build_initial_esp(unsigned char* stack_base, void (*entry)(void)) {
    struct task_stack_frame* frame = (struct task_stack_frame*)(stack_base + 4096 - sizeof(struct task_stack_frame));

    // 通用寄存器初值统一置 0，让首次切入时现场可预测
    frame->edi = 0;
    frame->esi = 0;
    frame->ebp = 0;
    frame->esp_placeholder = 0;
    frame->ebx = 0;
    frame->edx = 0;
    frame->ecx = 0;
    frame->eax = 0;

    // iretd 依赖这三个字段恢复执行点和标志位，使任务像一次中断返回那样启动
    frame->eip = (unsigned int)entry;
    frame->cs = 0x00000008;
    frame->eflags = 0x00000202;

    return (unsigned int)frame;
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

// 把被抢占任务的最新 ESP 写回 TCB；只要记住这个入口地址，就能找回整份现场
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
