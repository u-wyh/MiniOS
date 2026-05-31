// shell_elf_source.c：用户态最小 shell 的顶层入口，用于重新生成嵌入式 shell_elf.inc。
// 当前保留单一源文件入口，但把实现按语法解析、重定向、pipe、内建命令等职责拆到更小片段中。
#include "fs.h"
#include "user_program.h"

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_FORK 5
#define SYS_WAITPID 6
#define SYS_READ_CHAR 8
#define SYS_EXEC_ARGS 11
#define SYS_PS 12
#define SYS_KILL 13
#define SYS_WAIT_ANY 14
#define SYS_YIELD 15
#define SYS_SLEEP 16
#define SYS_SLEEP_PID 17
#define SYS_SET_BACKGROUND 18
#define SYS_GET_TICKS 19
#define SYS_CLEAR_SCREEN 20
#define SYS_OPEN 21
#define SYS_READ 22
#define SYS_CLOSE 23
#define SYS_FILE_COUNT 24
#define SYS_FILE_INFO 25
#define SYS_STAT 26
#define SYS_TOUCH 27
#define SYS_WRITEFILE 28
#define SYS_RM 29
#define SYS_APPEND_FILE 32
#define SYS_SET_STDOUT_REDIRECT 33
#define SYS_SET_STDIN_REDIRECT 34
#define SYS_PIPE_RESET 35
#define SYS_SET_STDOUT_PIPE 36
#define SYS_SET_STDIN_PIPE 37
#define SYS_SET_LAUNCH_READY 38
#define SYS_GET_LAUNCH_READY 39
// 当前内核默认把 PIT 配置为 20Hz，因此 1 tick 约等于 50ms。
#define SHELL_UPTIME_TICKS_PER_SECOND 20

#define PROCESS_NAME_MAX_LEN 16
#define SHELL_ARGV_MAX (USER_PROGRAM_MAX_ARGS + 1)
#define SHELL_LINE_MAX 128
#define SHELL_CAT_CHUNK_SIZE 32
#define SHELL_WRITEFILE_MAX_LEN (MAX_RAMFS_FILE_SIZE + 1)
// 与内核当前教学版 fd 上限保持一致，仅供用户态 shell 内部 fdtest 使用。
#define SHELL_FDTEST_MAX_OPEN 8

struct process_info {
    int pid;
    int ppid;
    int state;
    unsigned int age_ticks;
    unsigned int runs;
    int exit_status;
    int is_background;
    char name[PROCESS_NAME_MAX_LEN];
};

// 用户态 shell 复用同一份只读文件清单，但只保留显示所需的路径/内容/大小字段。
struct shell_builtin_text_file {
    const char* path;
    const char* content;
    unsigned int size;
};

// 教学版 run 重定向解析结果：仅供 shell 内部在创建用户进程前同时保存 stdin/stdout 配置。
struct shell_redirect_info {
    int has_stdin;
    int stdin_index;
    const char* stdin_path;
    int has_stdout;
    int stdout_index;
    int stdout_append;
    const char* stdout_path;
    int first_redirect_index;
};

/*
 * 组织说明：
 * 1. shared_data.inc：只读共享静态数据。
 * 2. syscalls.inc：最小 syscall 包装。
 * 3. parse_helpers.inc：字符串/argv/重定向与 pipe 语法辅助。
 * 4. redirect_validation.inc：run 重定向与 echo 重定向校验。
 * 5. basic_helpers.inc：数值、文件名、程序名等基础辅助。
 * 6. launch_helpers.inc：交互输入、程序启动与参数校验。
 * 7. pipe_exec.inc：教学版单管道执行模型。
 * 8. builtins.inc：shell 内建命令。
 * 9. main_loop.inc：shell 主循环。
 */

#include "shell_source_parts/shared_data.inc"
#include "shell_source_parts/syscalls.inc"
#include "shell_source_parts/parse_helpers.inc"
#include "shell_source_parts/redirect_validation.inc"
#include "shell_source_parts/basic_helpers.inc"
#include "shell_source_parts/launch_helpers.inc"
#include "shell_source_parts/pipe_exec.inc"
#include "shell_source_parts/builtins.inc"
#include "shell_source_parts/main_loop.inc"
