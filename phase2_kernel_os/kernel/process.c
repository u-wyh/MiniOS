#include "elf.h"
#include "fs.h"
#include "paging.h"
#include "process.h"
#include "user.h"
#include "vga.h"

#define PROCESS_MAX 16
#define USER_STACK_TOP 0x00800000

// 汇编入口：通过 iret 切换到用户态并从指定入口开始执行
extern void enter_user_mode(unsigned int user_entry, unsigned int user_stack_top);

// 最小进程表与当前进程指针
static struct process process_table[PROCESS_MAX];
static struct process* current_process = (struct process*)0;
static int next_pid = 1;

// 裸机环境下手动打印无符号整数
static void process_print_uint(unsigned int value) {
    char digits[16];
    int index = 0;

    if (value == 0) {
        print_char('0');
        return;
    }

    while (value > 0) {
        digits[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (index > 0) {
        index--;
        print_char(digits[index]);
    }
}

// 状态码转可读字符串，供 ps 输出
const char* process_state_name(int state) {
    if (state == PROCESS_UNUSED) {
        return "UNUSED";
    }

    if (state == PROCESS_READY) {
        return "READY";
    }

    if (state == PROCESS_RUNNING) {
        return "RUNNING";
    }

    if (state == PROCESS_ZOMBIE) {
        return "ZOMBIE";
    }

    return "UNKNOWN";
}

// 申请一个空闲 PCB 槽位
static struct process* process_alloc_slot(void) {
    int i;

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state == PROCESS_UNUSED) {
            process_table[i].used = 1;
            return &process_table[i];
        }
    }

    return (struct process*)0;
}

// 初始化进程表：清空槽位并重置 PID 计数
void process_init(void) {
    int i;

    for (i = 0; i < PROCESS_MAX; i++) {
        process_table[i].pid = 0;
        process_table[i].state = PROCESS_UNUSED;
        process_table[i].esp = 0;
        process_table[i].eip = 0;
        process_table[i].exit_status = 0;
        process_table[i].name = (const char*)0;
        process_table[i].used = 0;
    }

    current_process = (struct process*)0;
    next_pid = 1;
}

// 按文件名创建进程：分配 PID，加载 ELF，初始化 PCB
struct process* process_create(const char* name) {
    struct file* target;
    struct process* proc;
    unsigned int entry;

    if (name == (const char*)0 || name[0] == '\0') {
        print_string("process: empty name\n");
        return (struct process*)0;
    }

    target = fs_find(name);
    if (target == (struct file*)0) {
        print_string("process: file not found\n");
        return (struct process*)0;
    }

    proc = process_alloc_slot();
    if (proc == (struct process*)0) {
        print_string("process: table full\n");
        return (struct process*)0;
    }

    // 复用现有用户空间初始化，确保用户栈与映射就绪
    user_space_init();

    entry = elf_load((const unsigned char*)target->data, (unsigned int)target->size);
    if (entry == 0) {
        proc->used = 0;
        proc->state = PROCESS_UNUSED;
        print_string("process: elf load failed\n");
        return (struct process*)0;
    }

    proc->pid = next_pid++;
    proc->state = PROCESS_READY;
    proc->eip = entry;
    proc->esp = USER_STACK_TOP;
    proc->exit_status = 0;
    proc->name = target->name;

    return proc;
}

// 运行指定进程：更新状态并进入用户态执行
void process_run(struct process* proc) {
    if (proc == (struct process*)0 || proc->state != PROCESS_READY) {
        return;
    }

    if (current_process != (struct process*)0 && current_process->state == PROCESS_RUNNING) {
        current_process->state = PROCESS_READY;
    }

    current_process = proc;
    current_process->state = PROCESS_RUNNING;

    enter_user_mode(current_process->eip, current_process->esp);
}

// 将当前进程标记为 ZOMBIE，等待 shell wait 命令回收 PCB 槽位
void process_exit(int status) {
    if (current_process == (struct process*)0) {
        return;
    }

    current_process->exit_status = status;
    current_process->state = PROCESS_ZOMBIE;
    current_process = (struct process*)0;
}

// 回收一个 ZOMBIE 进程；本轮只释放 PCB 记录，不释放页表/用户栈等完整资源
int process_wait(void) {
    int i;
    int pid;

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state != PROCESS_ZOMBIE) {
            continue;
        }

        pid = process_table[i].pid;
        process_table[i].pid = 0;
        process_table[i].state = PROCESS_UNUSED;
        process_table[i].esp = 0;
        process_table[i].eip = 0;
        process_table[i].exit_status = 0;
        process_table[i].name = (const char*)0;
        process_table[i].used = 0;
        return pid;
    }

    return -1;
}

// 返回当前进程 pid；没有当前进程时返回 0
int process_current_pid(void) {
    if (current_process == (struct process*)0 || current_process->state != PROCESS_RUNNING) {
        return 0;
    }

    return current_process->pid;
}

// 输出进程列表：PID、STATE 与程序名
void process_list(void) {
    int i;

    print_string("PID   STATE    STATUS  NAME\n");
    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state == PROCESS_UNUSED) {
            continue;
        }

        process_print_uint((unsigned int)process_table[i].pid);
        print_string("    ");
        print_string(process_state_name(process_table[i].state));
        print_string("    ");
        process_print_uint((unsigned int)process_table[i].exit_status);
        print_string("       ");
        if (process_table[i].name != (const char*)0) {
            print_string(process_table[i].name);
        } else {
            print_string("(none)");
        }
        print_char('\n');
    }
}

// 返回已创建的进程数量
int process_count(void) {
    int i;
    int count = 0;

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state != PROCESS_UNUSED) {
            count++;
        }
    }

    return count;
}
