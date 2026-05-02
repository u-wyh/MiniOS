#include "elf.h"
#include "fs.h"
#include "mm.h"
#include "paging.h"
#include "process.h"
#include "vga.h"

#define PROCESS_MAX 16
#define USER_STACK_TOP 0x00800000
#define USER_STACK_SIZE 4096

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

// 当前没有真实 init 进程时，内核控制台统一用 pid 0 作为父进程
static int process_parent_pid_for_current_context(void) {
    if (current_process != (struct process*)0) {
        return current_process->pid;
    }

    return 0;
}

// 清空一个 PCB 槽位；本阶段只回收记录，不释放页表和用户栈等完整资源
static void process_clear_slot(struct process* proc) {
    unsigned int i;

    proc->pid = 0;
    proc->parent_pid = 0;
    proc->state = PROCESS_UNUSED;
    proc->esp = 0;
    proc->eip = 0;
    proc->exit_status = 0;
    proc->user_stack_va = 0;
    proc->user_stack_pa = 0;
    proc->user_stack_pages = 0;
    proc->user_page_count = 0;
    for (i = 0; i < ELF_LOAD_MAX_PAGES; i++) {
        proc->user_page_vaddr[i] = 0;
        proc->user_page_paddr[i] = 0;
    }
    proc->name = (const char*)0;
    proc->used = 0;
}

// 释放进程占用的最小用户资源：用户栈页 + ELF 映射页
static void process_free_resources(struct process* proc) {
    unsigned int i;

    if (proc == (struct process*)0) {
        return;
    }

    // 先释放用户栈页，再解除映射，防止 wait/waitpid 回收后残留无效映射
    if (proc->user_stack_pages != 0 && proc->user_stack_pa != 0 && proc->user_stack_va != 0) {
        unmap_page(proc->user_stack_va);
        free_page((void*)proc->user_stack_pa);
        proc->user_stack_va = 0;
        proc->user_stack_pa = 0;
        proc->user_stack_pages = 0;
    }

    // 逐页释放 ELF 代码/数据段物理页，并撤销虚拟地址映射
    for (i = 0; i < proc->user_page_count && i < ELF_LOAD_MAX_PAGES; i++) {
        if (proc->user_page_vaddr[i] != 0) {
            unmap_page(proc->user_page_vaddr[i]);
        }
        if (proc->user_page_paddr[i] != 0) {
            free_page((void*)proc->user_page_paddr[i]);
        }
        proc->user_page_vaddr[i] = 0;
        proc->user_page_paddr[i] = 0;
    }
    proc->user_page_count = 0;
}

// 按 pid 查找进程，waitpid 需要先确认目标是否仍在进程表中
static struct process* process_find_by_pid(int pid) {
    int i;

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state != PROCESS_UNUSED && process_table[i].pid == pid) {
            return &process_table[i];
        }
    }

    return (struct process*)0;
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
        process_clear_slot(&process_table[i]);
    }

    current_process = (struct process*)0;
    next_pid = 1;
}

// 按文件名创建进程：分配 PID，加载 ELF，初始化 PCB
struct process* process_create(const char* name) {
    struct file* target;
    struct process* proc;
    struct elf_load_info load_info;
    unsigned char* stack_page;
    unsigned int entry;
    unsigned int i;

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

    // 每个进程独立分配一页用户栈，供 wait/waitpid 回收阶段释放
    stack_page = (unsigned char*)alloc_page();
    if (stack_page == (unsigned char*)0) {
        process_clear_slot(proc);
        print_string("process: alloc stack failed\n");
        return (struct process*)0;
    }
    map_page(USER_STACK_TOP - USER_STACK_SIZE, (unsigned int)stack_page, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    entry = elf_load_with_info((const unsigned char*)target->data, (unsigned int)target->size, &load_info);
    if (entry == 0) {
        unmap_page(USER_STACK_TOP - USER_STACK_SIZE);
        free_page((void*)stack_page);
        process_clear_slot(proc);
        print_string("process: elf load failed\n");
        return (struct process*)0;
    }

    proc->pid = next_pid++;
    // 记录创建者：shell/内核上下文创建的进程暂时归属于 pid 0
    proc->parent_pid = process_parent_pid_for_current_context();
    proc->state = PROCESS_READY;
    proc->eip = entry;
    proc->esp = USER_STACK_TOP;
    proc->exit_status = 0;
    proc->user_stack_va = USER_STACK_TOP - USER_STACK_SIZE;
    proc->user_stack_pa = (unsigned int)stack_page;
    proc->user_stack_pages = 1;
    proc->user_page_count = load_info.page_count;
    for (i = 0; i < load_info.page_count && i < ELF_LOAD_MAX_PAGES; i++) {
        proc->user_page_vaddr[i] = load_info.page_vaddr[i];
        proc->user_page_paddr[i] = load_info.page_paddr[i];
    }
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

// 回收一个 ZOMBIE 子进程；本轮 wait 非阻塞，只回收已经退出的子进程
int process_wait(void) {
    int i;
    int pid;
    int parent_pid = process_parent_pid_for_current_context();

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state != PROCESS_ZOMBIE) {
            continue;
        }

        if (process_table[i].parent_pid != parent_pid) {
            continue;
        }

        pid = process_table[i].pid;
        // wait 时先释放子进程资源，再回收 PCB
        process_free_resources(&process_table[i]);
        process_clear_slot(&process_table[i]);
        return pid;
    }

    return -1;
}

// waitpid 雏形：只做立即检查，不阻塞等待子进程退出
int process_waitpid(int pid) {
    struct process* target = process_find_by_pid(pid);
    int parent_pid = process_parent_pid_for_current_context();

    if (target == (struct process*)0) {
        return -1;
    }

    if (target->parent_pid != parent_pid) {
        return -2;
    }

    if (target->state != PROCESS_ZOMBIE) {
        return -3;
    }

    // waitpid 回收前释放用户页资源，确保 PCB 复用时不会累计泄漏
    process_free_resources(target);
    process_clear_slot(target);
    return pid;
}

// 返回当前进程 pid；没有当前进程时返回 0
int process_current_pid(void) {
    if (current_process == (struct process*)0 || current_process->state != PROCESS_RUNNING) {
        return 0;
    }

    return current_process->pid;
}

// 输出进程列表：PID、PPID、STATE、退出码与程序名
void process_list(void) {
    int i;

    print_string("PID   PPID   STATE    STATUS  NAME\n");
    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state == PROCESS_UNUSED) {
            continue;
        }

        process_print_uint((unsigned int)process_table[i].pid);
        print_string("    ");
        process_print_uint((unsigned int)process_table[i].parent_pid);
        print_string("      ");
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
