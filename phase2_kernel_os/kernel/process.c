// process.c：进程子系统顶层入口。
// 当前仍保留单一编译入口，但把实现按生命周期、fd、重定向、pipe、fork/wait 等职责拆到更小片段文件中。
#include "elf.h"
#include "fs.h"
#include "mm.h"
#include "paging.h"
#include "pit.h"
#include "process.h"
#include "user_program.h"
#include "vga.h"

#define PROCESS_MAX 16
#define USER_STACK_TOP 0x00800000
#define USER_STACK_SIZE 4096
#define DEBUG_FORK 0
#define DEBUG_EXEC 0

// 汇编入口：通过 iret 切换到用户态并从指定入口开始执行
extern void enter_user_mode(unsigned int user_entry, unsigned int user_stack_top);

// 最小进程表与当前进程指针
static struct process process_table[PROCESS_MAX];
static struct process* current_process = (struct process*)0;
static struct process* last_exited_process = (struct process*)0;
static struct process_pipe_buffer process_pipe_buffer;
static int next_pid = 1;
// 记录教学版 init 进程 pid：用于孤儿进程 reparent，默认 -1 表示尚未建立 init
static int init_pid = -1;

// 前向声明：最小 PCB 分配逻辑在后文定义，供创建阶段复用
static struct process* process_alloc_slot(void);
// 前向声明：按进程重新安装用户页映射，供 fork/waitpid 恢复运行时切换用户镜像
static void process_activate_user_image(struct process* proc);
// 前向声明：fork 调试辅助函数会复用基础整数输出
static void process_print_uint(unsigned int value);
// 前向声明：教学版 argv 操作辅助函数，负责清空/复制 PCB 参数暂存区
static void process_clear_user_args(struct process* proc);
static void process_clear_fd_table(struct process* proc);
static int process_copy_user_args(struct process* proc, int argc, const char* const* argv);
static int process_name_equals(const char* a, const char* b);
static int process_copy_path(char* dst, const char* src, unsigned int max_len);
static int process_text_length(const char* text);
static int process_fd_slot_from_number(int fd);
static struct process_fd_entry* process_fd_get_entry(struct process* proc, int fd);
static int process_alloc_fd_slot(struct process* proc);
static int process_alloc_pipe_fd(struct process* proc, int fd_type);
static void process_fd_reset_slot(struct process* proc, int slot);
static int process_pipe_has_read_reference(void);
static int process_pipe_has_write_reference(void);
static int process_close_fd_for_proc(struct process* proc, int fd);
static void process_close_all_fds_for_proc(struct process* proc);
static void process_copy_fd_table(struct process* child, const struct process* parent);
static int process_fd_dup2(struct process* proc, int oldfd, int newfd);
static int process_open_file_for_proc(struct process* proc, const char* path);
static int process_open_file_write_for_proc(struct process* proc, const char* path);
static int process_read_pipe_fd(struct process* proc, int fd, char* user_buf, int size);
static int process_write_pipe_fd(struct process* proc, int fd, const char* user_buf, int size);
static int process_user_range_is_accessible(struct process* proc, uint32_t address, uint32_t size);
static void process_restore_current_user_mapping(void);
static void process_reparent_children(int old_parent_pid);
static int process_has_blocked_waiter(struct process* child);
static void process_reap_init_zombies(void);
static void process_release_user_image(struct process* proc);
static struct process* process_pick_next_ready(struct process* current);
static struct process* process_find_by_pid(int pid);
static int process_set_stdout_redirect(struct process* proc, const char* path, int is_append);
static int process_set_stdin_redirect(struct process* proc, const char* path);
static void process_pipe_buffer_clear(void);
static int process_is_init_waiting_shell_restart(struct process* child, struct process* parent);
static void process_record_schedule(struct process* proc);

/*
 * 组织说明：
 * 1. core_helpers.inc：进程表、PCB、用户镜像与基础辅助函数。
 * 2. exec_create.inc：exec / create / argv 查询相关逻辑。
 * 3. runtime_wait_sleep.inc：run / exit / wait / yield / sleep 等运行期生命周期。
 * 4. fd_and_input.inc：fd 打开读写关闭，以及 read_char/用户存在性等输入辅助。
 * 5. redirect_pipe.inc：后台标记、stdin/stdout 重定向、pipe buffer 与 launch_ready。
 * 6. fork_and_reporting.inc：fork、阻塞 waitpid、kill、resume、ps 与调度 tick。
 */

#include "process_parts/core_helpers.inc"
#include "process_parts/exec_create.inc"
#include "process_parts/runtime_wait_sleep.inc"
#include "process_parts/fd_and_input.inc"
#include "process_parts/redirect_pipe.inc"
#include "process_parts/fork_and_reporting.inc"
