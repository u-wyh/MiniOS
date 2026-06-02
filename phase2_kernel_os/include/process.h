// process.h：定义最小 PCB、进程状态和 create/exec/fork/wait 相关接口
#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include "elf.h"
#include "fs.h"
#include "syscall.h"
#include "user_program.h"

// 进程状态：空闲、就绪、运行、僵尸
#define PROCESS_UNUSED 0
#define PROCESS_READY 1
#define PROCESS_RUNNING 2
#define PROCESS_ZOMBIE 3
#define PROCESS_BLOCKED 4
#define PROCESS_SLEEPING 5

// 教学版 argv 上限：沿用统一用户程序参数约束，避免 shell / process 各自维护魔法数字。
#define PROCESS_MAX_USER_ARGS USER_PROGRAM_MAX_ARGS
#define PROCESS_MAX_ARG_LEN USER_PROGRAM_MAX_ARG_LEN
#define PROCESS_NAME_MAX_LEN 16
// 教学版每进程最大打开文件数：fd 从 3 开始分配，0/1/2 预留给 stdin/stdout/stderr。
#define PROCESS_MAX_OPEN_FILES 8
// 教学版文件描述符起始编号。
#define PROCESS_FD_BASE 3
// 教学版 fd 类型：当前最小区分空槽位、普通文件、pipe 读端和 pipe 写端。
#define PROCESS_FD_TYPE_NONE 0
#define PROCESS_FD_TYPE_FILE 1
#define PROCESS_FD_TYPE_PIPE_READ 2
#define PROCESS_FD_TYPE_PIPE_WRITE 3
// init 的父进程约定：0 表示没有普通父进程，是 MiniOS 进程树的根。
#define PROCESS_ROOT_PARENT_PID 0
// 教学版 kill 退出码：只表示“被 kill 终止”，不是 Unix/Linux 的信号编号。
#define PROCESS_KILL_EXIT_STATUS -9
// 教学版单管道缓冲区上限：左侧程序 stdout 全部先写入这里；当前固定容量，不做动态扩容。
#define PROCESS_PIPE_BUFFER_SIZE 512

// 用户态 ps 使用的进程只读摘要：避免直接暴露内核 PCB 结构
struct process_info {
    int pid;
    int ppid;
    int state;
    // age_ticks 表示当前 tick 与 create_tick 的差值，只反映进程已存在多久。
    uint32_t age_ticks;
    // runs 表示进程被调度器选中运行的次数；它不是 CPU 时间。
    uint32_t runs;
    // exit_status 记录最近一次退出码；对仍在运行的普通进程通常保持为 0。
    int exit_status;
    // is_background 表示该进程是否由 shell 以 start 方式作为后台任务启动。
    int is_background;
    char name[PROCESS_NAME_MAX_LEN];
};

// 教学版文件描述符表项：记录 fd 是否占用、当前类型、当前打开的是哪个路径、是否允许写入，以及当前偏移。
// 对 pipe fd 来说，path 当前为空字符串；真正的数据仍绑定到全局教学版 pipe buffer。
struct process_fd_entry {
    int used;
    int type;
    char path[MAX_FS_PATH_LEN];
    // can_write 为 1 表示该 fd 允许写入；当前只给 RAMFS 写打开路径设置，内置只读文件始终为 0。
    int can_write;
    uint32_t offset;
};

// 教学版单管道缓冲区：当前只支持一条前台 run A | run B，顺序执行而不是并发 pipe。
// active 为 1 表示 shell 正在执行一条教学版 pipe 命令；size 表示当前有效字节数；
// read_offset 表示右侧程序已经读取到的位置；overflowed 用于保证“buffer full”提示只输出一次。
struct process_pipe_buffer {
    int active;
    int used;
    int overflowed;
    char data[PROCESS_PIPE_BUFFER_SIZE];
    uint32_t size;
    uint32_t read_offset;
};

// 最小 PCB：保存进程身份、父子关系、状态与用户态入口现场
struct process {
    int pid;
    // parent_pid 表示创建该进程的父进程 pid；init 作为根进程使用 PROCESS_ROOT_PARENT_PID。
    int parent_pid;
    int state;
    uint32_t esp;
    uint32_t eip;
    int exit_status;
    // 记录用户栈资源：wait/waitpid 回收时释放该页
    uint32_t user_stack_va;
    uint32_t user_stack_pa;
    uint32_t user_stack_pages;
    uint32_t user_stack_flags;
    // 记录用户代码/数据页资源：由 ELF 装载时填写，回收时逐页释放
    uint32_t user_page_count;
    uint32_t user_page_vaddr[ELF_LOAD_MAX_PAGES];
    uint32_t user_page_paddr[ELF_LOAD_MAX_PAGES];
    uint32_t user_page_flags[ELF_LOAD_MAX_PAGES];
    // 保存需要恢复到用户态的最小中断现场，供 fork 子进程与阻塞 waitpid 恢复执行
    struct interrupt_frame saved_frame;
    int has_saved_frame;
    int waiting_pid;
    // 仅在 SLEEPING 状态下生效：记录该进程应被唤醒的最小 tick
    uint32_t wakeup_tick;
    // create_tick 表示进程创建时的系统 tick，供 ps 统计存活时间；它不是 CPU 运行时间。
    uint32_t create_tick;
    // schedule_count 表示进程被调度器选中运行的次数；它不是 CPU 使用率，也不是精确运行时间。
    uint32_t schedule_count;
    // 教学版 argv 暂存区：exec_args 先把参数复制到 PCB，中小规模参数足够支撑当前实验
    int user_argc;
    char user_argv[PROCESS_MAX_USER_ARGS][PROCESS_MAX_ARG_LEN];
    // 记录本次退出是否来自用户态 shell 主动执行 exit 命令，供 init 决定是否自动重启 shell
    int requested_exit;
    // 教学版每进程 fd 表：当前最小支持普通文件 fd 与教学版 pipe fd，不实现 dup / dup2 / 共享引用计数。
    struct process_fd_entry fd_table[PROCESS_MAX_OPEN_FILES];
    // stdout_redirect_enabled 为 1 时，当前进程后续 SYS_WRITE 将写入 RAMFS 文件而不是前台屏幕。
    int stdout_redirect_enabled;
    // stdout_redirect_append 为 1 表示 shell 使用 >>；为 0 表示 shell 使用 >。
    int stdout_redirect_append;
    // stdout_redirect_started 用于区分第一次写入：> 首次覆盖写，后续多次 SYS_WRITE 统一改为追加。
    int stdout_redirect_started;
    // stdout_redirect_path 保存 stdout 重定向目标；必须复制到 PCB，不能引用 shell 临时 token 缓冲区。
    char stdout_redirect_path[MAX_FS_PATH_LEN];
    // stdin_redirect_enabled 为 1 时，当前进程的 SYS_READ(fd=0) 将从指定文件读取，而不是走默认空输入语义。
    int stdin_redirect_enabled;
    // stdin_redirect_path 保存 stdin 重定向源文件路径；当前允许内置只读文件和 RAMFS 文件。
    char stdin_redirect_path[MAX_FS_PATH_LEN];
    // stdin_redirect_offset 记录当前从 stdin 文件已经读取到的位置，EOF 后 SYS_READ(fd=0) 返回 0。
    uint32_t stdin_redirect_offset;
    // stdin_pipe_fd / stdout_pipe_fd 表示当前进程绑定到教学版 pipe 的 fd 端点；-1 表示未绑定。
    int stdin_pipe_fd;
    int stdout_pipe_fd;
    // stdout_redirect_to_pipe / stdin_redirect_from_pipe 目前保留为兼容字段：
    // Task83 之后，shell pipe 连接已经优先通过内核内部 fd_dup2(oldfd, 0/1) 绑定到标准入口；
    // 这些标记更多用于兼容旧路径、教学版 SYS_WRITE/SYS_READ 入口与文档表达。
    int stdout_redirect_to_pipe;
    int stdin_redirect_from_pipe;
    // launch_ready 为 0 时，shell 仍在给 fork 子进程补齐重定向/pipe 配置；就绪前不应开始执行子分支。
    int launch_ready;

    // 扩展字段：记录程序名与槽位占用，便于 ps 展示与管理
    const char* name;
    int is_background;
    int used;
};

// 初始化进程表与 PID 分配器
void process_init(void);
// 按文件名创建并装载一个用户进程
struct process* process_create(const char* name);
// 把一份 ELF 镜像装载到指定进程中，成功后更新入口、用户栈和资源记录
int process_exec(struct process* proc, const unsigned char* elf_data, unsigned int elf_size);
// 按文件名装载或替换指定进程的用户镜像，供后续 exec 语义复用
int process_exec_file(struct process* proc, const char* name);
// 运行指定进程（切到用户态入口）
void process_run(struct process* proc);
// 将当前进程标记为 ZOMBIE，并保存退出码
void process_exit(int status);
// 回收一个当前父进程名下的 ZOMBIE 子进程，成功返回 pid，失败返回 -1
int process_wait(void);
// 尝试回收指定 pid 的 ZOMBIE 子进程，返回 pid 或错误码
int process_waitpid(int pid);
// 非阻塞回收任意一个当前进程名下的 ZOMBIE 子进程；成功返回 pid，无可回收时返回 0
int process_wait_any(void);
// 用户态 yield：当前进程主动让出 CPU，成功切换返回 -4，未切换返回 0
int process_yield_syscall(struct interrupt_frame* frame, struct interrupt_frame** next_frame);
// 用户态 sleep：当前进程睡眠指定 tick，到期前不参与调度
int process_sleep_syscall(unsigned int ticks, struct interrupt_frame* frame, struct interrupt_frame** next_frame);
// 在 PIT tick 驱动下唤醒到期睡眠进程：仅把 SLEEPING 进程改回 READY
void process_wakeup_sleeping(unsigned int now_tick);
// 按 pid 将目标进程设置为 SLEEPING，供 shell 的 sleep <pid> <ticks> 调试命令使用
int process_sleep_pid(int pid, unsigned int ticks);
// 打开一个内置只读文本文件，成功返回 fd，失败返回负值。
int process_open_file(const char* path);
// 以写模式打开一个 RAMFS 文本文件，成功返回 fd，失败返回负值。
int process_open_file_write(const char* path);
// 从已打开 fd 读取数据到用户缓冲区，成功返回读取字节数，EOF 返回 0。
int process_read_file(int fd, char* user_buf, int size);
// 向已打开的可写 fd 写入数据，成功返回写入字节数，失败返回负值。
int process_write_file(int fd, const char* user_buf, int size);
// 关闭一个已打开 fd，成功返回 0，失败返回负值。
int process_close_file(int fd);
// 用户态 read_char 在无输入时进入最小阻塞语义：保存现场并切换到其他 READY 进程
int process_read_char_syscall(struct interrupt_frame* frame, struct interrupt_frame** next_frame);
// 键盘 IRQ 到来时唤醒一个正在等待输入的进程，并把字符写入其 syscall 返回值
int process_wake_read_char_waiter(char ch);
// 按 pid 设置后台标记：教学版 start 用它表示“后台运行但不占用前台输出”
int process_set_background_by_pid(int pid, int is_background);
// 状态码转可读字符串，供 ps 和文档对照使用
const char* process_state_name(int state);
// 返回当前进程 pid；无当前进程返回 0
int process_current_pid(void);
// 返回当前进程是否被标记为后台任务
int process_current_is_background(void);
// 为指定 pid 的进程配置教学版 stdout 重定向：后续该进程的 SYS_WRITE 将根据该配置写 RAMFS。
int process_set_stdout_redirect_by_pid(int pid, const char* path, int is_append);
// 为指定 pid 的进程配置教学版 stdin 重定向：后续该进程的 SYS_READ(fd=0) 将根据该配置从文件读取。
int process_set_stdin_redirect_by_pid(int pid, const char* path);
// 为指定 pid 的进程启用“stdout 写入教学版 pipe buffer”模式。
int process_set_stdout_pipe_by_pid(int pid);
// 为指定 pid 的进程启用“stdin 从教学版 pipe buffer 读取”模式。
int process_set_stdin_pipe_by_pid(int pid);
// 为指定 pid 的 shell 子进程解除启动门闩，让它进入 READY 并允许被调度。
int process_mark_launch_ready_by_pid(int pid);
// 返回当前运行进程的启动门闩状态；仅供 shell 子分支在 exec 前等待父进程配置完成。
int process_current_launch_ready(void);
// 返回当前进程是否启用了 stdout 重定向。
int process_current_has_stdout_redirect(void);
// 返回当前进程是否启用了 stdout -> pipe。
int process_current_has_stdout_pipe(void);
// 把当前进程的一次 SYS_WRITE 文本输出写到 RAMFS 重定向目标；成功返回写入字节数。
int process_write_stdout_redirect(const char* text);
// 把当前进程的一次 SYS_WRITE 文本输出写到教学版 pipe buffer；成功返回写入字节数。
int process_write_stdout_pipe(const char* text);
// 清空教学版单管道缓冲区，供 shell 在执行 run A | run B 前重置状态。
void process_pipe_reset(void);
// 为当前进程创建一对教学版 pipe fd，并把 read/write 两端回填到用户态 int fds[2]。
int process_pipe_create_fds(int* user_fds);
// 为当前进程执行教学版 dup2：成功返回 newfd，失败返回 -1。
int process_dup2(int oldfd, int newfd);
// 返回是否存在正在等待键盘输入的用户进程，供键盘 IRQ 区分用户 shell 与内核 shell
int process_has_read_char_waiter(void);
// 返回进程表中是否仍有用户进程存在；键盘 IRQ 用它避免用户态运行期间误回内核 shell
int process_has_user_process(void);
// 将当前运行进程标记为“主动请求退出”，供 init 区分正常 exit 与异常退出
void process_mark_current_requested_exit(void);
// 基于当前系统调用现场复制一个教学版子进程，成功返回子进程 pid
int process_fork(struct interrupt_frame* frame);
// 用户态 waitpid：目标未退出时阻塞父进程并切换到子进程运行
int process_waitpid_syscall(int pid, struct interrupt_frame* frame, struct interrupt_frame** next_frame);
// 教学版 kill：将目标普通用户进程标记为 ZOMBIE，退出码由调用方给定（如 PROCESS_KILL_EXIT_STATUS）
int process_kill(int pid, int exit_code);
// 当前运行进程按固定 program_id 执行最小 exec 替换，成功时直接改写返回现场
int process_exec_program(int program_id, struct interrupt_frame* frame);
// 当前运行进程执行带教学版 argv 的最小 exec：参数先复制到 PCB 暂存区，再替换用户镜像
int process_exec_program_args(int program_id, int argc, const char* const* argv, struct interrupt_frame* frame);
// 返回当前进程保存的教学版 argc；当前无进程时返回错误
int process_get_argc(void);
// 将当前进程保存的 argv[index] 复制到用户缓冲区，成功返回字符串长度
int process_get_arg(int index, char* user_buf, int max_len);
// 处理当前进程 exit 后的后续恢复：若存在阻塞父进程则返回其用户态现场
struct interrupt_frame* process_resume_after_exit(void);
// 输出进程列表（PID / PPID / STATE）
void process_list(void);
// 按活动进程序号读取一条只读摘要；成功返回 0，越界返回 -1
int process_get_info_by_index(int index, struct process_info* out);
// 返回已创建的进程数量
int process_count(void);
// 供 PIT 时间片调度使用：保存当前运行进程现场并切换到下一个 READY 进程，返回应恢复的中断现场栈顶
unsigned int process_schedule_tick(unsigned int current_esp);

#endif
