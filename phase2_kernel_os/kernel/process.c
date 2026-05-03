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
static struct process* last_exited_process = (struct process*)0;
static int next_pid = 1;

// 前向声明：最小 PCB 分配逻辑在后文定义，供创建阶段复用
static struct process* process_alloc_slot(void);
// 前向声明：按进程重新安装用户页映射，供 fork/waitpid 恢复运行时切换用户镜像
static void process_activate_user_image(struct process* proc);

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

    if (state == PROCESS_BLOCKED) {
        return "BLOCKED";
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
    proc->user_stack_flags = 0;
    proc->user_page_count = 0;
    for (i = 0; i < ELF_LOAD_MAX_PAGES; i++) {
        proc->user_page_vaddr[i] = 0;
        proc->user_page_paddr[i] = 0;
        proc->user_page_flags[i] = 0;
    }
    proc->has_saved_frame = 0;
    proc->waiting_pid = 0;
    proc->name = (const char*)0;
    proc->used = 0;
}

// 在共享页表模型里，把某个进程记录的用户页重新安装到固定用户虚拟地址
static void process_activate_user_image(struct process* proc) {
    unsigned int i;

    if (proc == (struct process*)0) {
        return;
    }

    if (proc->user_stack_pages != 0 && proc->user_stack_pa != 0 && proc->user_stack_va != 0) {
        map_page(proc->user_stack_va, proc->user_stack_pa, proc->user_stack_flags);
    }

    for (i = 0; i < proc->user_page_count && i < ELF_LOAD_MAX_PAGES; i++) {
        if (proc->user_page_vaddr[i] == 0 || proc->user_page_paddr[i] == 0) {
            continue;
        }

        map_page(proc->user_page_vaddr[i], proc->user_page_paddr[i], proc->user_page_flags[i]);
    }
}

// 裸机环境下手动复制字节，供 fork 复制用户页和保存 trapframe 时复用
static void process_copy_bytes(unsigned char* dst, const unsigned char* src, unsigned int size) {
    unsigned int i;

    for (i = 0; i < size; i++) {
        dst[i] = src[i];
    }
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
        proc->user_page_flags[i] = 0;
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
    proc->user_stack_flags = PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    proc->user_page_count = load_info->page_count;

    for (i = 0; i < load_info->page_count && i < ELF_LOAD_MAX_PAGES; i++) {
        proc->user_page_vaddr[i] = load_info->page_vaddr[i];
        proc->user_page_paddr[i] = load_info->page_paddr[i];
        proc->user_page_flags[i] = load_info->page_flags[i];
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

// 为阻塞 waitpid 查找目标子进程，既允许 READY，也允许已经成为 ZOMBIE
static struct process* process_find_child_for_parent(int pid, int parent_pid) {
    struct process* target = process_find_by_pid(pid);

    if (target == (struct process*)0) {
        return (struct process*)0;
    }

    if (target->parent_pid != parent_pid) {
        return (struct process*)0;
    }

    return target;
}

// 复制父进程已记录的最小用户镜像：逐页分配新物理页并拷贝代码/数据/用户栈内容
static int process_copy_user_image(struct process* child, struct process* parent) {
    unsigned int i;
    unsigned char* page;

    if (child == (struct process*)0 || parent == (struct process*)0) {
        return -1;
    }

    child->user_stack_va = parent->user_stack_va;
    child->user_stack_pages = parent->user_stack_pages;
    child->user_stack_flags = parent->user_stack_flags;
    child->user_page_count = parent->user_page_count;

    if (parent->user_stack_pages != 0 && parent->user_stack_pa != 0) {
        page = (unsigned char*)alloc_page();
        if (page == (unsigned char*)0) {
            process_release_user_image(child);
            return -2;
        }

        process_copy_bytes(page, (const unsigned char*)parent->user_stack_pa, USER_STACK_SIZE);
        child->user_stack_pa = (unsigned int)page;
    }

    for (i = 0; i < parent->user_page_count && i < ELF_LOAD_MAX_PAGES; i++) {
        child->user_page_vaddr[i] = parent->user_page_vaddr[i];
        child->user_page_flags[i] = parent->user_page_flags[i];

        if (parent->user_page_paddr[i] == 0) {
            continue;
        }

        page = (unsigned char*)alloc_page();
        if (page == (unsigned char*)0) {
            process_release_user_image(child);
            return -3;
        }

        process_copy_bytes(page, (const unsigned char*)parent->user_page_paddr[i], 4096);
        child->user_page_paddr[i] = (unsigned int)page;
    }

    child->eip = parent->eip;
    child->esp = parent->esp;
    return 0;
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
    last_exited_process = (struct process*)0;
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
    proc->has_saved_frame = 0;
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
    process_activate_user_image(current_process);

    enter_user_mode(current_process->eip, current_process->esp);
}

// 将当前进程标记为 ZOMBIE，等待 shell wait 命令回收 PCB 槽位
void process_exit(int status) {
    if (current_process == (struct process*)0) {
        return;
    }

    current_process->exit_status = status;
    current_process->state = PROCESS_ZOMBIE;
    last_exited_process = current_process;
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
        if (last_exited_process == &process_table[i]) {
            last_exited_process = (struct process*)0;
        }
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
    if (last_exited_process == target) {
        last_exited_process = (struct process*)0;
    }
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

// 基于当前运行进程的 trapframe 构造一个教学版 fork 子进程
int process_fork(struct interrupt_frame* frame) {
    struct process* parent = current_process;
    struct process* child;
    int copy_result;

    if (parent == (struct process*)0 || frame == (struct interrupt_frame*)0) {
        return -1;
    }

    child = process_create_object();
    if (child == (struct process*)0) {
        return -2;
    }

    child->parent_pid = parent->pid;
    child->name = parent->name;
    child->exit_status = 0;

    copy_result = process_copy_user_image(child, parent);
    if (copy_result != 0) {
        process_release_user_image(child);
        process_clear_slot(child);
        return -3;
    }

    // 子进程从和父进程相同的用户态返回点继续执行，但 fork 返回值必须是 0
    child->saved_frame = *frame;
    child->saved_frame.eax = 0;
    child->has_saved_frame = 1;
    child->state = PROCESS_READY;
    return child->pid;
}

// 用户态 waitpid：目标未退出时阻塞父进程，并切换到子进程继续执行
int process_waitpid_syscall(int pid, struct interrupt_frame* frame, struct interrupt_frame** next_frame) {
    struct process* parent = current_process;
    struct process* child;
    int result;

    if (next_frame != (struct interrupt_frame**)0) {
        *next_frame = (struct interrupt_frame*)0;
    }

    if (parent == (struct process*)0 || frame == (struct interrupt_frame*)0) {
        return -1;
    }

    result = process_waitpid(pid);
    if (result >= 0) {
        return result;
    }

    if (result != -3) {
        return result;
    }

    child = process_find_child_for_parent(pid, parent->pid);
    if (child == (struct process*)0 || child->state != PROCESS_READY || child->has_saved_frame == 0) {
        return -3;
    }

    // waitpid 在教学版 fork 中采用最小阻塞语义：父进程保存现场后，把 CPU 让给子进程
    parent->saved_frame = *frame;
    parent->has_saved_frame = 1;
    parent->waiting_pid = pid;
    parent->state = PROCESS_BLOCKED;

    current_process = child;
    child->state = PROCESS_RUNNING;
    process_activate_user_image(child);
    if (next_frame != (struct interrupt_frame**)0) {
        *next_frame = &child->saved_frame;
    }

    return -4;
}

// 子进程 exit 后，若父进程正阻塞等待它，则直接回收子进程并恢复父进程
struct interrupt_frame* process_resume_after_exit(void) {
    struct process* child;
    struct process* parent;
    int child_pid;
    int i;

    child = last_exited_process;
    child_pid = 0;

    if (child == (struct process*)0 || child->state != PROCESS_ZOMBIE || child->pid == 0) {
        return (struct interrupt_frame*)0;
    }

    child_pid = child->pid;

    for (i = 0; i < PROCESS_MAX; i++) {
        if (process_table[i].state != PROCESS_BLOCKED) {
            continue;
        }

        if (process_table[i].pid != child->parent_pid) {
            continue;
        }

        if (process_table[i].waiting_pid != child_pid || process_table[i].has_saved_frame == 0) {
            continue;
        }

        parent = &process_table[i];
        parent->saved_frame.eax = (unsigned int)child_pid;
        parent->waiting_pid = 0;

        process_release_user_image(child);
        process_clear_slot(child);
        last_exited_process = (struct process*)0;

        current_process = parent;
        parent->state = PROCESS_RUNNING;
        process_activate_user_image(parent);
        return &parent->saved_frame;
    }

    return (struct interrupt_frame*)0;
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
