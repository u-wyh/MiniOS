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

// 前向声明：最小 PCB 分配逻辑在后文定义，供创建阶段复用
static struct process* process_alloc_slot(void);

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

// 判断进程当前是否已经持有一份用户镜像，供 exec 替换语义决定是否先释放旧资源
static int process_has_user_image(struct process* proc) {
    if (proc == (struct process*)0) {
        return 0;
    }

    if (proc->user_stack_pages != 0) {
        return 1;
    }

    if (proc->user_page_count != 0) {
        return 1;
    }

    if (proc->eip != 0 || proc->esp != 0) {
        return 1;
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

// 释放进程占用的最小用户镜像资源：用户栈页 + ELF 映射页
static void process_release_user_image(struct process* proc) {
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
    proc->eip = 0;
    proc->esp = 0;
}

// 创建一个最小进程对象：只分配 PCB、PID 和父子关系，不负责装载 ELF
static struct process* process_create_object(void) {
    struct process* proc = process_alloc_slot();

    if (proc == (struct process*)0) {
        return (struct process*)0;
    }

    process_clear_slot(proc);
    proc->used = 1;
    proc->pid = next_pid++;
    proc->parent_pid = process_parent_pid_for_current_context();
    proc->state = PROCESS_UNUSED;
    return proc;
}

// 把 ELF 装载结果写回 PCB，统一记录入口、栈和用户页资源范围
static void process_commit_exec_image(struct process* proc, unsigned int entry, unsigned char* stack_page, struct elf_load_info* load_info) {
    unsigned int i;

    proc->eip = entry;
    proc->esp = USER_STACK_TOP;
    proc->user_stack_va = USER_STACK_TOP - USER_STACK_SIZE;
    proc->user_stack_pa = (unsigned int)stack_page;
    proc->user_stack_pages = 1;
    proc->user_page_count = load_info->page_count;

    for (i = 0; i < load_info->page_count && i < ELF_LOAD_MAX_PAGES; i++) {
        proc->user_page_vaddr[i] = load_info->page_vaddr[i];
        proc->user_page_paddr[i] = load_info->page_paddr[i];
    }
}

// 执行一次教学版镜像替换：先释放旧用户空间，再加载新的 ELF；失败时暂不回滚旧镜像
static int process_replace_image(struct process* proc, const unsigned char* elf_data, unsigned int elf_size) {
    process_release_user_image(proc);
    return process_exec(proc, elf_data, elf_size);
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

// 把一份 ELF 镜像装载到指定进程中：负责用户栈、ELF 页和入口现场初始化
int process_exec(struct process* proc, const unsigned char* elf_data, unsigned int elf_size) {
    struct elf_load_info load_info;
    unsigned char* stack_page;
    unsigned int entry;

    if (proc == (struct process*)0 || elf_data == (const unsigned char*)0 || elf_size == 0) {
        return -1;
    }

    // process_exec 只负责把一份新镜像装到空进程对象；已有镜像时必须先走替换语义
    if (process_has_user_image(proc) != 0) {
        return -4;
    }

    // exec 成功后必须有一份新的用户栈，因此先准备栈页再加载 ELF
    stack_page = (unsigned char*)alloc_page();
    if (stack_page == (unsigned char*)0) {
        return -2;
    }

    map_page(USER_STACK_TOP - USER_STACK_SIZE, (unsigned int)stack_page, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    entry = elf_load_with_info(elf_data, elf_size, &load_info);
    if (entry == 0) {
        // 装载失败时只回滚本次新建的栈页；旧镜像是否还能恢复由上层语义决定
        unmap_page(USER_STACK_TOP - USER_STACK_SIZE);
        free_page((void*)stack_page);
        return -3;
    }

    process_commit_exec_image(proc, entry, stack_page, &load_info);
    proc->exit_status = 0;
    return 0;
}

// 按文件名装载或替换指定进程的用户镜像，供 process_create 与未来 syscall exec 复用
int process_exec_file(struct process* proc, const char* name) {
    struct file* target;

    if (proc == (struct process*)0 || name == (const char*)0 || name[0] == '\0') {
        return -1;
    }

    target = fs_find(name);
    if (target == (struct file*)0) {
        return -2;
    }

    if (process_has_user_image(proc) != 0) {
        if (process_replace_image(proc, (const unsigned char*)target->data, (unsigned int)target->size) != 0) {
            return -3;
        }
    } else {
        if (process_exec(proc, (const unsigned char*)target->data, (unsigned int)target->size) != 0) {
            return -3;
        }
    }

    proc->name = target->name;
    return 0;
}

// 按文件名创建进程：先创建 PCB，再调用 exec 语义装载用户镜像
struct process* process_create(const char* name) {
    struct process* proc;
    int exec_result;

    if (name == (const char*)0 || name[0] == '\0') {
        print_string("process: empty name\n");
        return (struct process*)0;
    }

    proc = process_create_object();
    if (proc == (struct process*)0) {
        print_string("process: table full\n");
        return (struct process*)0;
    }

    exec_result = process_exec_file(proc, name);
    if (exec_result != 0) {
        if (exec_result == -2) {
            print_string("process: file not found\n");
        } else {
            print_string("process: exec load failed\n");
        }
        process_release_user_image(proc);
        process_clear_slot(proc);
        return (struct process*)0;
    }
    proc->state = PROCESS_READY;
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
        process_release_user_image(&process_table[i]);
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
    process_release_user_image(target);
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
