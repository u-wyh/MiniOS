// mini_pipeline_elf_source.c：教学版用户态固定 pipeline 入口，复用 pipe + fork + dup2 + exec(argv)

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_FORK 5
#define SYS_WAITPID 6
#define SYS_GET_ARGC 9
#define SYS_GET_ARG 10
#define SYS_EXEC_ARGS 11
#define SYS_CLOSE 23
#define SYS_PIPE 40
#define SYS_DUP2 41

#include "../../include/user_program.h"

#define MINI_PIPELINE_ARG_BUF_LEN USER_PROGRAM_MAX_ARG_LEN
#define MINI_PIPELINE_MAX_CMDS USER_PROGRAM_MAX_ARGS

// 教学版 pipeline 命令段：记录这一段自己的 argc / argv 以及解析出的目标程序编号。
struct mini_pipeline_cmd {
    int argc;
    int program_id;
    const char* argv[USER_PROGRAM_MAX_ARGS];
};

struct mini_pipeline_program_entry {
    const char* name;
    int program_id;
};

#define MINI_PIPELINE_PROGRAM_ROW(symbol, value, name, shell_visible) {name, symbol},

// 统一复用内核维护的程序清单，避免 mini_pipeline 再散落一份手写 program_id 表。
static const struct mini_pipeline_program_entry mini_pipeline_program_table[] = {
    MINIOS_USER_PROGRAM_LIST(MINI_PIPELINE_PROGRAM_ROW)
};

#undef MINI_PIPELINE_PROGRAM_ROW

// 向当前 stdout 输出一段文本。
static void user_write(const char* text) {
    __asm__ __volatile__(
        "int $0x80"
        :
        : "a"(SYS_WRITE), "b"(text)
        : "memory");
}

// 结束当前用户态程序。
static void user_exit(int status) {
    __asm__ __volatile__(
        "int $0x80"
        :
        : "a"(SYS_EXIT), "b"(status)
        : "memory");

    for (;;) {
    }
}

// fork 当前进程。
static int user_fork(void) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_FORK)
        : "memory");
    return result;
}

// 等待指定子进程退出。
static int user_waitpid(int pid) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_WAITPID), "b"(pid)
        : "memory");
    return result;
}

// 返回当前程序保存的教学版参数数量。
static int user_get_argc(void) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_GET_ARGC)
        : "memory");
    return result;
}

// 把指定参数复制到缓冲区，成功返回长度。
static int user_get_arg(int index, char* buffer, int max_len) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_GET_ARG), "b"(index), "c"(buffer), "d"(max_len)
        : "memory");
    return result;
}

// 以当前教学版带参数接口执行最小 exec。
static int user_exec_args(int program_id, int argc, const char* const* argv) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_EXEC_ARGS), "b"(program_id), "c"(argc), "d"(argv)
        : "memory");
    return result;
}

// 关闭一个教学版 fd。
static int user_close(int fd) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_CLOSE), "b"(fd)
        : "memory");
    return result;
}

// 创建一对教学版 pipe fd。
static int user_pipe(int* fds) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_PIPE), "b"(fds)
        : "memory");
    return result;
}

// 执行教学版 dup2。
static int user_dup2(int oldfd, int newfd) {
    int result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_DUP2), "b"(oldfd), "c"(newfd)
        : "memory");
    return result;
}

// 输出十进制整数，便于错误提示。
static void user_write_int(int value) {
    char digits[16];
    int index = 0;
    unsigned int number;

    if (value == 0) {
        user_write("0");
        return;
    }

    if (value < 0) {
        user_write("-");
        number = (unsigned int)(-value);
    } else {
        number = (unsigned int)value;
    }

    while (number > 0) {
        digits[index++] = (char)('0' + (number % 10));
        number /= 10;
    }

    while (index > 0) {
        char one[2];
        index--;
        one[0] = digits[index];
        one[1] = '\0';
        user_write(one);
    }
}

// 判断两个最小 ASCII 字符串是否相等。
static int mini_pipeline_string_equals(const char* left, const char* right) {
    if (left == (const char*)0 || right == (const char*)0) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        if (*left != *right) {
            return 0;
        }
        left++;
        right++;
    }

    return (*left == '\0' && *right == '\0') ? 1 : 0;
}

// 按名称查 program_id，供 mini_pipeline 在用户态解析 left/right 程序名。
static int mini_pipeline_program_id_from_name(const char* name) {
    int index;
    int count = (int)(sizeof(mini_pipeline_program_table) / sizeof(mini_pipeline_program_table[0]));

    for (index = 0; index < count; index++) {
        if (mini_pipeline_string_equals(name, mini_pipeline_program_table[index].name) != 0) {
            return mini_pipeline_program_table[index].program_id;
        }
    }

    return 0;
}

// 输出统一错误提示。
static void mini_pipeline_write_error(const char* message, int code) {
    user_write("mini_pipeline: ");
    user_write(message);
    if (code != 0) {
        user_write(" (");
        user_write_int(code);
        user_write(")");
    }
    user_write("\n");
}

// 输出当前最小用法，强调用 -- 分隔左右命令。
static void mini_pipeline_write_usage(void) {
    user_write("usage: mini_pipeline <cmd1> [args...] -- <cmd2> [args...] [-- <cmd3> [args...] ...]\n");
}

// 从当前程序 argv 里读取一个参数到固定缓冲区。
static int mini_pipeline_load_arg(int index, char* buffer) {
    return user_get_arg(index, buffer, MINI_PIPELINE_ARG_BUF_LEN);
}

// 关闭 mini_pipeline 当前持有的所有 pipe fd：父进程和子进程都复用这一逻辑，避免多余端点阻碍 EOF。
static void mini_pipeline_close_all_pipes(int pipe_count, int pipe_fds[MINI_PIPELINE_MAX_CMDS - 1][2]) {
    int index;

    for (index = 0; index < pipe_count; index++) {
        user_close(pipe_fds[index][0]);
        user_close(pipe_fds[index][1]);
    }
}

// 解析多段 mini_pipeline 参数：
// 1. 用 -- 分隔命令段
// 2. 每段至少一个 token
// 3. 每段都保留自己的 argv
// 4. -- 本身不进入任何命令段
static int mini_pipeline_parse_args(
    int argc,
    struct mini_pipeline_cmd* cmds,
    int* cmd_count_out,
    char storage[MINI_PIPELINE_MAX_CMDS][USER_PROGRAM_MAX_ARGS][MINI_PIPELINE_ARG_BUF_LEN]) {
    static char token[MINI_PIPELINE_ARG_BUF_LEN];
    int cmd_index = 0;
    int arg_index = 0;
    int saw_separator = 0;
    int index;

    if (cmds == (struct mini_pipeline_cmd*)0 || cmd_count_out == (int*)0) {
        return -1;
    }

    for (index = 0; index < MINI_PIPELINE_MAX_CMDS; index++) {
        cmds[index].argc = 0;
        cmds[index].program_id = 0;
    }

    for (index = 1; index < argc; index++) {
        if (mini_pipeline_load_arg(index, token) <= 0) {
            return -2;
        }

        if (mini_pipeline_string_equals(token, "--") != 0) {
            saw_separator = 1;
            if (arg_index == 0) {
                return -3;
            }
            cmds[cmd_index].argc = arg_index;
            cmd_index++;
            if (cmd_index >= MINI_PIPELINE_MAX_CMDS) {
                return -4;
            }
            arg_index = 0;
            continue;
        }

        if (arg_index >= USER_PROGRAM_MAX_ARGS) {
            return -5;
        }

        if (cmd_index >= MINI_PIPELINE_MAX_CMDS) {
            return -4;
        }

        if (mini_pipeline_load_arg(index, storage[cmd_index][arg_index]) <= 0) {
            return -2;
        }
        cmds[cmd_index].argv[arg_index] = storage[cmd_index][arg_index];
        arg_index++;
    }

    if (arg_index == 0) {
        return -3;
    }

    cmds[cmd_index].argc = arg_index;
    cmd_index++;

    if (saw_separator == 0 || cmd_index < 2) {
        return -6;
    }

    *cmd_count_out = cmd_index;
    return 0;
}

// 根据命令名查找每一段的 program_id；只要有未知程序就直接报错退出。
static int mini_pipeline_resolve_program_ids(struct mini_pipeline_cmd* cmds, int cmd_count) {
    int index;

    for (index = 0; index < cmd_count; index++) {
        cmds[index].program_id = mini_pipeline_program_id_from_name(cmds[index].argv[0]);
        if (cmds[index].program_id == PROGRAM_INVALID || cmds[index].program_id == 0) {
            return index + 1;
        }
    }

    return 0;
}

// 主流程：采用教学版最小多级并发 pipeline，N 个命令使用 N-1 个 pipe 串联。
void _start(void) {
    static struct mini_pipeline_cmd cmds[MINI_PIPELINE_MAX_CMDS];
    static char storage[MINI_PIPELINE_MAX_CMDS][USER_PROGRAM_MAX_ARGS][MINI_PIPELINE_ARG_BUF_LEN];
    static int pipe_fds[MINI_PIPELINE_MAX_CMDS - 1][2];
    static int child_pids[MINI_PIPELINE_MAX_CMDS];
    int argc;
    int cmd_count;
    int parse_result;
    int resolve_result;
    int pipe_index;
    int child_index;
    int created_pipes = 0;
    int started_children = 0;
    int wait_result;
    int fork_result;

    argc = user_get_argc();
    if (argc < 4) {
        mini_pipeline_write_usage();
        user_exit(1);
    }

    if (argc > USER_PROGRAM_MAX_ARGS) {
        mini_pipeline_write_error("too many args", 0);
        user_exit(1);
    }

    parse_result = mini_pipeline_parse_args(argc, cmds, &cmd_count, storage);
    if (parse_result != 0) {
        mini_pipeline_write_usage();
        user_exit(1);
    }

    resolve_result = mini_pipeline_resolve_program_ids(cmds, cmd_count);
    if (resolve_result != 0) {
        mini_pipeline_write_error("unknown program", resolve_result);
        user_exit(1);
    }

    user_write("mini_pipeline: start\n");

    for (pipe_index = 0; pipe_index < cmd_count - 1; pipe_index++) {
        parse_result = user_pipe(pipe_fds[pipe_index]);
        if (parse_result != 0) {
            mini_pipeline_close_all_pipes(created_pipes, pipe_fds);
            mini_pipeline_write_error("pipe failed", parse_result);
            user_exit(1);
        }
        created_pipes++;
    }

    for (child_index = 0; child_index < cmd_count; child_index++) {
        fork_result = user_fork();
        if (fork_result < 0) {
            mini_pipeline_close_all_pipes(created_pipes, pipe_fds);
            mini_pipeline_write_error("fork failed", fork_result);
            while (started_children > 0) {
                started_children--;
                user_waitpid(child_pids[started_children]);
            }
            user_exit(1);
        }

        if (fork_result == 0) {
            int dup_result;

            if (child_index > 0) {
                dup_result = user_dup2(pipe_fds[child_index - 1][0], 0);
                if (dup_result != 0) {
                    user_exit(10 + child_index);
                }
            }

            if (child_index < cmd_count - 1) {
                dup_result = user_dup2(pipe_fds[child_index][1], 1);
                if (dup_result != 1) {
                    user_exit(20 + child_index);
                }
            }

            // 子进程完成 dup2 之后，必须关闭自己手里的所有原始 pipe fd，
            // 否则多余写端会让下游永远等不到 EOF。
            mini_pipeline_close_all_pipes(created_pipes, pipe_fds);
            if (user_exec_args(cmds[child_index].program_id, cmds[child_index].argc, cmds[child_index].argv) != 0) {
                user_exit(30 + child_index);
            }

            user_exit(40 + child_index);
        }

        child_pids[started_children] = fork_result;
        started_children++;
    }

    // 父进程不参与真正的数据读写，因此在所有子进程建立完后立即关闭自己的 pipe 端点。
    mini_pipeline_close_all_pipes(created_pipes, pipe_fds);

    for (child_index = 0; child_index < cmd_count; child_index++) {
        wait_result = user_waitpid(child_pids[child_index]);
        if (wait_result != child_pids[child_index]) {
            mini_pipeline_write_error("wait failed", wait_result);
            user_exit(1);
        }
    }

    user_write("mini_pipeline: ok\n");
    user_exit(0);
}
