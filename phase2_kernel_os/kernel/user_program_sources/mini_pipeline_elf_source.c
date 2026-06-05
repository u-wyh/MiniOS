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
    user_write("usage: mini_pipeline <left_prog> [left_args...] -- <right_prog> [right_args...]\n");
}

// 从当前程序 argv 里读取一个参数到固定缓冲区。
static int mini_pipeline_load_arg(int index, char* buffer) {
    return user_get_arg(index, buffer, MINI_PIPELINE_ARG_BUF_LEN);
}

// 在 mini_pipeline 的 argv 中查找 -- 分隔符，返回所在下标，失败返回 -1。
static int mini_pipeline_find_separator(int argc) {
    static char token[MINI_PIPELINE_ARG_BUF_LEN];
    int index;

    for (index = 1; index < argc; index++) {
        if (mini_pipeline_load_arg(index, token) <= 0) {
            return -1;
        }
        if (mini_pipeline_string_equals(token, "--") != 0) {
            return index;
        }
    }

    return -1;
}

// 根据 argv 的一段连续区间构造某一侧 exec 要使用的 argc/argv。
static int mini_pipeline_build_side_argv(
    int begin_index,
    int end_index,
    char storage[USER_PROGRAM_MAX_ARGS][MINI_PIPELINE_ARG_BUF_LEN],
    const char* argv_out[USER_PROGRAM_MAX_ARGS]) {
    int argc;
    int index;

    if (begin_index >= end_index) {
        return -1;
    }

    argc = end_index - begin_index;
    if (argc > USER_PROGRAM_MAX_ARGS) {
        return -2;
    }

    for (index = 0; index < argc; index++) {
        if (mini_pipeline_load_arg(begin_index + index, storage[index]) <= 0) {
            return -3;
        }
        argv_out[index] = storage[index];
    }

    return argc;
}

// 主流程：采用教学版最小并发 pipeline，左右两侧都先 fork 出来，再由父进程分别 wait。
void _start(void) {
    static char left_storage[USER_PROGRAM_MAX_ARGS][MINI_PIPELINE_ARG_BUF_LEN];
    static char right_storage[USER_PROGRAM_MAX_ARGS][MINI_PIPELINE_ARG_BUF_LEN];
    static const char* right_argv[USER_PROGRAM_MAX_ARGS];
    static const char* left_argv[USER_PROGRAM_MAX_ARGS];
    int argc;
    int left_argc;
    int right_argc;
    int separator_index;
    int left_program_id;
    int right_program_id;
    int build_result;
    int fds[2];
    int pipe_result;
    int writer_pid;
    int reader_pid;
    int wait_result;
    int dup_result;

    argc = user_get_argc();
    if (argc < 4) {
        mini_pipeline_write_usage();
        user_exit(1);
    }

    if (argc > USER_PROGRAM_MAX_ARGS) {
        mini_pipeline_write_error("too many args", 0);
        user_exit(1);
    }

    separator_index = mini_pipeline_find_separator(argc);
    if (separator_index < 0) {
        mini_pipeline_write_usage();
        user_exit(1);
    }

    build_result = mini_pipeline_build_side_argv(1, separator_index, left_storage, left_argv);
    if (build_result <= 0) {
        mini_pipeline_write_usage();
        user_exit(1);
    }
    left_argc = build_result;

    build_result = mini_pipeline_build_side_argv(separator_index + 1, argc, right_storage, right_argv);
    if (build_result <= 0) {
        mini_pipeline_write_usage();
        user_exit(1);
    }
    right_argc = build_result;

    left_program_id = mini_pipeline_program_id_from_name(left_storage[0]);
    right_program_id = mini_pipeline_program_id_from_name(right_storage[0]);
    if (left_program_id == PROGRAM_INVALID || left_program_id == 0) {
        mini_pipeline_write_error("unknown left program", 0);
        user_exit(1);
    }
    if (right_program_id == PROGRAM_INVALID || right_program_id == 0) {
        mini_pipeline_write_error("unknown right program", 0);
        user_exit(1);
    }

    user_write("mini_pipeline: start\n");

    pipe_result = user_pipe(fds);
    if (pipe_result != 0) {
        mini_pipeline_write_error("pipe failed", pipe_result);
        user_exit(1);
    }

    writer_pid = user_fork();
    if (writer_pid < 0) {
        mini_pipeline_write_error("fork left failed", writer_pid);
        user_exit(1);
    }

    if (writer_pid == 0) {
        dup_result = user_dup2(fds[1], 1);
        if (dup_result != 1) {
            user_exit(2);
        }

        user_close(fds[0]);
        user_close(fds[1]);
        if (user_exec_args(left_program_id, left_argc, left_argv) != 0) {
            user_exit(3);
        }

        user_exit(4);
    }

    reader_pid = user_fork();
    if (reader_pid < 0) {
        user_close(fds[0]);
        user_close(fds[1]);
        mini_pipeline_write_error("fork right failed", reader_pid);
        user_exit(1);
    }

    if (reader_pid == 0) {
        dup_result = user_dup2(fds[0], 0);
        if (dup_result != 0) {
            user_exit(5);
        }

        user_close(fds[0]);
        user_close(fds[1]);
        if (user_exec_args(right_program_id, right_argc, right_argv) != 0) {
            user_exit(6);
        }

        user_exit(7);
    }

    // 父进程不参与真正的数据读写，因此在两个子进程都建立完后立即关闭自己的 pipe 端点。
    // 这样 reader 才能在 writer 退出后正确观察到 write end 关闭并最终得到 EOF。
    user_close(fds[0]);
    user_close(fds[1]);

    wait_result = user_waitpid(writer_pid);
    if (wait_result != writer_pid) {
        mini_pipeline_write_error("wait left failed", wait_result);
        user_exit(1);
    }

    wait_result = user_waitpid(reader_pid);
    if (wait_result != reader_pid) {
        mini_pipeline_write_error("wait right failed", wait_result);
        user_exit(1);
    }

    user_write("mini_pipeline: ok\n");
    user_exit(0);
}
