// shell_elf_source.c：用户态最小 shell 源文件，用于重新生成嵌入式 shell_elf.inc
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

// 用户态 shell 侧的教学版只读文件视图：直接复用共享宏定义，避免 ls/cat 各自维护散落文件名。
static const struct shell_builtin_text_file shell_builtin_files[] = {
#define SHELL_BUILD_TEXT_FILE(path_text, content_text) \
    {path_text, content_text, (unsigned int)(sizeof(content_text) - 1)},
    MINIOS_BUILTIN_TEXT_FILE_LIST(SHELL_BUILD_TEXT_FILE)
#undef SHELL_BUILD_TEXT_FILE
};

// 最小 int 0x80 包装：保持和当前教学版用户程序 ABI 一致。
static int user_syscall0(int number) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(number) : "memory");
    return result;
}

// 一个参数的系统调用封装。
static int user_syscall1(int number, int arg1) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(number), "b"(arg1) : "memory");
    return result;
}

// 两个参数的系统调用封装。
static int user_syscall2(int number, int arg1, int arg2) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2) : "memory");
    return result;
}

// 三个参数的系统调用封装。
static int user_syscall3(int number, int arg1, int arg2, int arg3) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3) : "memory");
    return result;
}

// 向控制台输出字符串。
static void user_write(const char* text) {
    user_syscall1(SYS_WRITE, (int)text);
}

// 退出当前用户进程。
static void user_exit(int status) {
    user_syscall1(SYS_EXIT, status);
    for (;;) {
    }
}

// 读取一个字符；无输入时内核会阻塞当前 shell。
static int user_read_char(void) {
    return user_syscall0(SYS_READ_CHAR);
}

// 让当前进程睡眠若干 tick。
static int user_sleep_ticks(unsigned int ticks) {
    return user_syscall1(SYS_SLEEP, (int)ticks);
}

// 教学调试接口：按 pid 让目标进程睡眠。
static int user_sleep_pid(int pid, unsigned int ticks) {
    return user_syscall2(SYS_SLEEP_PID, pid, (int)ticks);
}

// 读取系统启动以来的 tick 数。
static unsigned int user_get_ticks(void) {
    return (unsigned int)user_syscall0(SYS_GET_TICKS);
}

// 创建子进程。
static int user_fork(void) {
    return user_syscall0(SYS_FORK);
}

// 等待指定子进程退出并回收。
static int user_waitpid(int pid) {
    return user_syscall1(SYS_WAITPID, pid);
}

// 非阻塞回收任意一个已经退出的子进程。
static int user_wait_any(void) {
    return user_syscall0(SYS_WAIT_ANY);
}

// 以带参数方式执行教学版最小 exec。
static int user_exec_args(int program_id, int argc, const char* const* argv) {
    return user_syscall3(SYS_EXEC_ARGS, program_id, argc, (int)argv);
}

// 逐条读取 ps 摘要。
static int user_ps_get(int index, struct process_info* info) {
    return user_syscall2(SYS_PS, index, (int)info);
}

// 终止指定 pid。
static int user_kill(int pid) {
    return user_syscall1(SYS_KILL, pid);
}

// 设置后台标记，供 start 命令使用。
static int user_set_background(int pid, int is_background) {
    return user_syscall2(SYS_SET_BACKGROUND, pid, is_background);
}

// 请求内核清空当前 VGA 文本屏幕，供用户态 clear 命令复用。
static void user_clear_screen(void) {
    user_syscall0(SYS_CLEAR_SCREEN);
}

// 打开一个教学版只读文件，成功返回 fd。
static int user_open(const char* path) {
    return user_syscall1(SYS_OPEN, (int)path);
}

// 从 fd 读取最多 size 字节到用户缓冲区，成功返回字节数，EOF 返回 0。
static int user_read(int fd, char* buffer, int size) {
    return user_syscall3(SYS_READ, fd, (int)buffer, size);
}

// 关闭一个已打开 fd。
static int user_close(int fd) {
    return user_syscall1(SYS_CLOSE, fd);
}

// 查询当前内置只读文件数量；供用户态 ls 程序与 shell 共享同一套教学版文件列表语义。
static int user_file_count(void) {
    return user_syscall0(SYS_FILE_COUNT);
}

// 按索引读取内置只读文件路径，并返回该文件大小。
static int user_file_info(int index, char* buffer, int max_len) {
    return user_syscall3(SYS_FILE_INFO, index, (int)buffer, max_len);
}

// 创建一个空 RAMFS 文件；供 touch 命令复用。
static int user_touch(const char* path) {
    return user_syscall1(SYS_TOUCH, (int)path);
}

// 覆盖写入一个 RAMFS 文本文件；供 writefile 命令复用。
static int user_writefile(const char* path, const char* text) {
    return user_syscall2(SYS_WRITEFILE, (int)path, (int)text);
}

// 删除一个 RAMFS 文件；供 rm 命令复用。
static int user_rm(const char* path) {
    return user_syscall1(SYS_RM, (int)path);
}

// 追加写入一个 RAMFS 文本文件；供 append 命令复用。
static int user_appendfile(const char* path, const char* text) {
    return user_syscall2(SYS_APPEND_FILE, (int)path, (int)text);
}

// 比较两个字符串是否相等。
static int shell_streq(const char* left, const char* right) {
    int i = 0;

    if (left == (const char*)0 || right == (const char*)0) {
        return 0;
    }

    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) {
            return 0;
        }
        i++;
    }

    return left[i] == '\0' && right[i] == '\0';
}

// 输出进程状态名：ps 和 jobs 共用同一套教学版状态展示，避免两处状态文案不一致。
static void shell_write_state(int state) {
    if (state == 1) {
        user_write("READY");
    } else if (state == 2) {
        user_write("RUNNING");
    } else if (state == 3) {
        user_write("ZOMBIE");
    } else if (state == 4) {
        user_write("BLOCKED");
    } else if (state == 5) {
        user_write("SLEEPING");
    } else {
        user_write("UNKNOWN");
    }
}

// 统计字符串长度；若长度达到上限仍未结束，则返回 -1 表示超长。
static int shell_string_length_with_limit(const char* text, int max_len) {
    int length = 0;

    if (text == (const char*)0 || max_len <= 0) {
        return -1;
    }

    while (text[length] != '\0') {
        if (length >= (max_len - 1)) {
            return -1;
        }
        length++;
    }

    return length;
}

// 把指定位置之后的参数用单个空格拼接成一段文本，供 writefile 最小复用。
static int shell_join_args(int argc, char** argv, int start_index, char* buffer, int max_len) {
    int i;
    int out = 0;

    if (buffer == (char*)0 || max_len <= 0) {
        return -1;
    }

    buffer[0] = '\0';
    for (i = start_index; i < argc; i++) {
        int j = 0;

        if (i > start_index) {
            if (out >= max_len - 1) {
                buffer[max_len - 1] = '\0';
                return -2;
            }
            buffer[out++] = ' ';
        }

        while (argv[i][j] != '\0') {
            if (out >= max_len - 1) {
                buffer[max_len - 1] = '\0';
                return -2;
            }
            buffer[out++] = argv[i][j++];
        }
    }

    buffer[out] = '\0';
    return out;
}

// 输出十进制整数，便于显示 pid/tick/age。
static void shell_write_uint(int value) {
    char digits[16];
    int count = 0;
    unsigned int current;

    if (value == 0) {
        user_write("0");
        return;
    }

    if (value < 0) {
        user_write("-");
        current = (unsigned int)(-value);
    } else {
        current = (unsigned int)value;
    }

    while (current != 0) {
        digits[count++] = (char)('0' + (current % 10));
        current /= 10;
    }

    while (count > 0) {
        char one[2];
        count--;
        one[0] = digits[count];
        one[1] = '\0';
        user_write(one);
    }
}

// 把十进制字符串解析成整数；失败返回 0。
static int shell_atoi(const char* text, int* out_value) {
    int value = 0;
    int i = 0;

    if (text == (const char*)0 || text[0] == '\0' || out_value == (int*)0) {
        return 0;
    }

    while (text[i] != '\0') {
        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }
        value = value * 10 + (text[i] - '0');
        i++;
    }

    *out_value = value;
    return 1;
}

// 教学版文件名匹配：兼容 `/readme.txt`、`readme.txt` 以及不便输入标点时的 `readmetxt` 简写。
static int shell_file_name_matches(const char* input, const char* path) {
    int input_index = 0;
    int path_index = 0;

    if (input == (const char*)0 || path == (const char*)0) {
        return 0;
    }

    if (input[0] == '/') {
        input_index++;
    }
    if (path[0] == '/') {
        path_index++;
    }

    for (;;) {
        while (input[input_index] == '.') {
            input_index++;
        }
        while (path[path_index] == '.') {
            path_index++;
        }

        if (input[input_index] == '\0' || path[path_index] == '\0') {
            break;
        }

        if (input[input_index] != path[path_index]) {
            return 0;
        }

        input_index++;
        path_index++;
    }

    while (input[input_index] == '.') {
        input_index++;
    }
    while (path[path_index] == '.') {
        path_index++;
    }

    return input[input_index] == '\0' && path[path_index] == '\0';
}

// 返回当前教学版只读文本文件数量；文件清单仍只维护在共享宏里一份。
static int shell_builtin_file_count(void) {
    return (int)(sizeof(shell_builtin_files) / sizeof(shell_builtin_files[0]));
}

// 按索引读取共享只读文本文件，供 ls 遍历输出。
static const struct shell_builtin_text_file* shell_builtin_file_at(int index) {
    if (index < 0 || index >= (int)(sizeof(shell_builtin_files) / sizeof(shell_builtin_files[0]))) {
        return (const struct shell_builtin_text_file*)0;
    }

    return &shell_builtin_files[index];
}

// 按路径查找共享只读文本文件，供 cat 输出内容。
static const struct shell_builtin_text_file* shell_builtin_file_find(const char* path) {
    int i;

    if (path == (const char*)0 || path[0] == '\0') {
        return (const struct shell_builtin_text_file*)0;
    }

    for (i = 0; i < shell_builtin_file_count(); i++) {
        const struct shell_builtin_text_file* file = shell_builtin_file_at(i);

        if (file != (const struct shell_builtin_text_file*)0 && shell_file_name_matches(path, file->path)) {
            return file;
        }
    }

    return (const struct shell_builtin_text_file*)0;
}

// 逐字符读入一行，并在本地回显。
static int shell_read_line(char* buffer, int capacity) {
    int length = 0;

    for (;;) {
        int ch = user_read_char();

        if (ch == '\r' || ch == '\n') {
            user_write("\n");
            buffer[length] = '\0';
            return length;
        }

        if (ch == '\b') {
            if (length > 0) {
                length--;
                user_write("\b \b");
            }
            continue;
        }

        if (ch <= 31 || ch > 126) {
            continue;
        }

        if ((length + 1) >= capacity) {
            continue;
        }

        buffer[length++] = (char)ch;
        {
            char one[2];
            one[0] = (char)ch;
            one[1] = '\0';
            user_write(one);
        }
    }
}

// 按空格或 tab 拆分参数，直接在原始缓冲区上写入 '\0'；若 token 数超过上限则返回 -1。
static int shell_split_line(char* line, char** argv, int max_argv) {
    int argc = 0;
    char* current = line;

    while (*current != '\0') {
        while (*current == ' ' || *current == '\t') {
            current++;
        }

        if (*current == '\0') {
            break;
        }

        if (argc >= max_argv) {
            return -1;
        }

        argv[argc++] = current;
        while (*current != '\0' && *current != ' ' && *current != '\t') {
            current++;
        }

        if (*current == '\0') {
            break;
        }

        *current = '\0';
        current++;
    }

    return argc;
}

// 通过共享用户程序清单做 name -> program_id 解析，避免 shell 再维护一份散落映射。
static int shell_program_id_from_name(const char* name) {
#define SHELL_MATCH_PROGRAM(symbol, value, program_name, visible) \
    if ((visible) != 0 && shell_streq(name, program_name)) {      \
        return symbol;                                             \
    }
    MINIOS_USER_PROGRAM_LIST(SHELL_MATCH_PROGRAM)
#undef SHELL_MATCH_PROGRAM

    // 兼容不便输入下划线的场景，允许用 sleeptest 作为 sleep_test 的简写别名。
    if (shell_streq(name, "sleeptest")) {
        return PROGRAM_SLEEP_TEST;
    }

    // 兼容不便输入下划线的场景，允许用 loopexit 作为 loop_exit 的简写别名。
    if (shell_streq(name, "loopexit")) {
        return PROGRAM_LOOP_EXIT;
    }

    return PROGRAM_INVALID;
}

// 校验即将传给用户程序的教学版 argv：当前保留 argv[0]=程序名，参数过多或过长时在 shell 侧先失败。
static int shell_validate_program_args(int argc, char** argv) {
    int i;

    if (argc < 0 || argc > USER_PROGRAM_MAX_ARGS) {
        return -1;
    }

    for (i = 0; i < argc; i++) {
        if (shell_string_length_with_limit(argv[i], USER_PROGRAM_MAX_ARG_LEN) < 0) {
            return -2;
        }
    }

    return 0;
}

// 统一处理 run/start/hello 的 fork + exec + waitpid 逻辑。
static int shell_spawn_program(int program_id, int argc, char** argv, int wait_child, int is_background) {
    int pid = user_fork();

    if (pid < 0) {
        user_write("Fork failed\n");
        return -1;
    }

    if (pid == 0) {
        int exec_result = user_exec_args(program_id, argc, (const char* const*)argv);

        if (exec_result != 0) {
            user_write("Exec failed\n");
            user_exit(99);
        }
        user_exit(0);
    }

    if (is_background != 0) {
        user_set_background(pid, 1);
    }

    if (wait_child != 0) {
        if (user_waitpid(pid) < 0) {
            user_write("Wait failed\n");
            return -1;
        }
        return 0;
    }

    user_write("Started pid: ");
    shell_write_uint(pid);
    user_write("\n");
    return pid;
}

// 输出 echo 的参数。
static void shell_cmd_echo(int argc, char** argv) {
    int i;

    if (argc <= 1) {
        user_write("\n");
        return;
    }

    for (i = 1; i < argc; i++) {
        user_write(argv[i]);
        if ((i + 1) < argc) {
            user_write(" ");
        }
    }
    user_write("\n");
}

// 输出当前 tick。
static void shell_cmd_uptime(void) {
    unsigned int ticks = user_get_ticks();
    unsigned int seconds = ticks / SHELL_UPTIME_TICKS_PER_SECOND;

    user_write("ticks: ");
    shell_write_uint((int)ticks);
    user_write(", seconds: ");
    shell_write_uint((int)seconds);
    user_write("\n");
}

// 输出教学版只读文件表：当前只列出内置文本文件，不做真实目录遍历。
static void shell_cmd_ls(int argc) {
    int i;
    int count;
    char path[MAX_FS_PATH_LEN];

    if (argc > 1) {
        user_write("ls: directory args not supported\n");
        return;
    }

    count = user_file_count();
    if (count < 0) {
        user_write("ls: count failed\n");
        return;
    }

    user_write("NAME              SIZE\n");
    for (i = 0; i < count; i++) {
        int size = user_file_info(i, path, MAX_FS_PATH_LEN);

        if (size < 0) {
            user_write("ls: info failed\n");
            return;
        }

        user_write(path);
        user_write("       ");
        shell_write_uint(size);
        user_write("\n");
    }
}

// 输出指定只读文本文件内容；当前优先通过 open/read/close 走教学版 fd 层。
static void shell_cmd_cat(int argc, char** argv) {
    char buffer[SHELL_CAT_CHUNK_SIZE + 1];
    int fd;
    int read_result;

    if (argc <= 1) {
        user_write("Usage: cat <file>\n");
        return;
    }

    fd = user_open(argv[1]);
    if (fd < 0) {
        user_write("cat: file not found\n");
        return;
    }

    for (;;) {
        read_result = user_read(fd, buffer, SHELL_CAT_CHUNK_SIZE);
        if (read_result < 0) {
            user_write("cat: read failed\n");
            user_close(fd);
            return;
        }

        if (read_result == 0) {
            break;
        }

        buffer[read_result] = '\0';
        user_write(buffer);
    }

    if (user_close(fd) < 0) {
        user_write("cat: close failed\n");
    }
}

// 创建一个空 RAMFS 文件；当前要求路径不存在，且不能覆盖内置只读文件。
static void shell_cmd_touch(int argc, char** argv) {
    int result;

    if (argc != 2) {
        user_write("Usage: touch <file>\n");
        return;
    }

    result = user_touch(argv[1]);
    if (result < 0) {
        user_write("touch failed\n");
    }
}

// 覆盖写入一个 RAMFS 文本文件；当前把剩余参数用空格拼成一段文本。
static void shell_cmd_writefile(int argc, char** argv) {
    char text[SHELL_WRITEFILE_MAX_LEN];
    int result;

    if (argc < 3) {
        user_write("Usage: writefile <file> <text>\n");
        return;
    }

    if (shell_join_args(argc, argv, 2, text, SHELL_WRITEFILE_MAX_LEN) < 0) {
        user_write("writefile: text too long\n");
        return;
    }

    result = user_writefile(argv[1], text);
    if (result < 0) {
        user_write("writefile failed\n");
    }
}

// 追加写入一个 RAMFS 文本文件；当前把剩余参数用空格拼成一段文本。
static void shell_cmd_append(int argc, char** argv) {
    char text[SHELL_WRITEFILE_MAX_LEN];
    int result;

    if (argc < 3) {
        user_write("Usage: append <file> <text>\n");
        return;
    }

    if (shell_join_args(argc, argv, 2, text, SHELL_WRITEFILE_MAX_LEN) < 0) {
        user_write("append: text too long\n");
        return;
    }

    result = user_appendfile(argv[1], text);
    if (result < 0) {
        user_write("append failed\n");
    }
}

// 删除一个 RAMFS 文件；当前禁止删除内置只读文件。
static void shell_cmd_rm(int argc, char** argv) {
    int result;

    if (argc != 2) {
        user_write("Usage: rm <file>\n");
        return;
    }

    result = user_rm(argv[1]);
    if (result < 0) {
        user_write("rm failed\n");
    }
}

// 内部 fd 自检：验证 EOF、close 后 read 失败，以及 fd 表满后 open 失败。
static void shell_cmd_fdtest(void) {
    char buffer[SHELL_CAT_CHUNK_SIZE + 1];
    int fds[SHELL_FDTEST_MAX_OPEN + 1];
    int i;
    int fd;
    int read_result;

    fd = user_open("readmetxt");
    if (fd < 0) {
        user_write("fdtest open failed\n");
        return;
    }

    for (;;) {
        read_result = user_read(fd, buffer, SHELL_CAT_CHUNK_SIZE);
        if (read_result < 0) {
            user_write("fdtest read failed\n");
            user_close(fd);
            return;
        }
        if (read_result == 0) {
            break;
        }
    }

    if (user_read(fd, buffer, SHELL_CAT_CHUNK_SIZE) != 0) {
        user_write("fdtest eof failed\n");
        user_close(fd);
        return;
    }

    if (user_close(fd) < 0) {
        user_write("fdtest close failed\n");
        return;
    }

    if (user_read(fd, buffer, SHELL_CAT_CHUNK_SIZE) >= 0) {
        user_write("fdtest read after close failed\n");
        return;
    }

    for (i = 0; i < SHELL_FDTEST_MAX_OPEN; i++) {
        fds[i] = user_open("programs");
        if (fds[i] < 0) {
            user_write("fdtest table fill failed\n");
            while (i > 0) {
                i--;
                user_close(fds[i]);
            }
            return;
        }
    }

    fds[SHELL_FDTEST_MAX_OPEN] = user_open("programs");
    if (fds[SHELL_FDTEST_MAX_OPEN] >= 0) {
        user_write("fdtest full check failed\n");
        for (i = 0; i <= SHELL_FDTEST_MAX_OPEN; i++) {
            user_close(fds[i]);
        }
        return;
    }

    for (i = 0; i < SHELL_FDTEST_MAX_OPEN; i++) {
        user_close(fds[i]);
    }

    user_write("fdtest ok\n");
}

// 输出 ps 结果，并显示 AGE / RUNS 列。
static void shell_cmd_ps(void) {
    int index = 0;
    struct process_info info;

    user_write("PID  PPID  STATE     AGE   RUNS  EXIT  NAME\n");
    while (user_ps_get(index, &info) == 0) {
        shell_write_uint(info.pid);
        user_write("    ");
        shell_write_uint(info.ppid);
        user_write("     ");

        shell_write_state(info.state);

        user_write("   ");
        shell_write_uint((int)info.age_ticks);
        user_write("   ");
        shell_write_uint((int)info.runs);
        user_write("   ");
        shell_write_uint(info.exit_status);
        user_write("   ");
        user_write(info.name);
        user_write("\n");
        index++;
    }
}

// 输出当前 shell 直接管理的后台子进程；jobs 只是观察，不负责回收资源。
static void shell_cmd_jobs(void) {
    int index = 0;
    int shell_pid = 0;
    int job_id = 1;
    struct process_info info;

    while (user_ps_get(index, &info) == 0) {
        if (info.state == 2 && shell_streq(info.name, "shell")) {
            shell_pid = info.pid;
            break;
        }
        index++;
    }

    if (shell_pid == 0) {
        user_write("No background jobs\n");
        return;
    }

    index = 0;
    while (user_ps_get(index, &info) == 0) {
        if (info.ppid == shell_pid && info.is_background != 0) {
            if (job_id == 1) {
                user_write("JOB  PID  STATE     NAME\n");
            }
            shell_write_uint(job_id);
            user_write("    ");
            shell_write_uint(info.pid);
            user_write("    ");
            shell_write_state(info.state);
            user_write("   ");
            user_write(info.name);
            user_write("\n");
            job_id++;
        }
        index++;
    }

    if (job_id == 1) {
        user_write("No background jobs\n");
    }
}

// 兼容保留 sleep <ticks> 和 sleep <pid> <ticks> 两种最小教学语义。
static void shell_cmd_sleep(int argc, char** argv) {
    int first = 0;
    int second = 0;

    if (argc <= 1) {
        user_write("Usage: sleep <ticks>\n");
        return;
    }

    if (argc == 2) {
        if (shell_atoi(argv[1], &first) == 0) {
            user_write("Invalid ticks\n");
            return;
        }

        if (first == 0) {
            return;
        }

        if (user_sleep_ticks((unsigned int)first) < 0) {
            user_write("Sleep failed\n");
        }
        return;
    }

    if (argc == 3) {
        if (shell_atoi(argv[1], &first) == 0 || shell_atoi(argv[2], &second) == 0) {
            user_write("Invalid ticks\n");
            return;
        }

        if (user_sleep_pid(first, (unsigned int)second) < 0) {
            user_write("Sleep failed\n");
        }
        return;
    }

    user_write("Usage: sleep <ticks>\n");
}

// 输出帮助信息。
static void shell_cmd_help(void) {
    user_write("commands:\n");
    user_write("  help\n");
    user_write("  clear\n");
    user_write("  echo <text>\n");
    user_write("  run <program> [args]\n");
    user_write("  start <program> [args]\n");
    user_write("  jobs\n");
    user_write("  wait [pid]\n");
    user_write("  kill <pid>\n");
    user_write("  ls\n");
    user_write("  cat <file>\n");
    user_write("  touch <file>\n");
    user_write("  writefile <file> <text>\n");
    user_write("  append <file> <text>\n");
    user_write("  rm <file>\n");
    user_write("  ps    show process table with age/runs\n");
    user_write("  uptime\n");
    user_write("  ticks\n");
    user_write("  sleep <ticks>\n");
    user_write("  hello\n");
    user_write("  exit\n");
    user_write("programs:\n");
    user_write("  hello echo ls cat stat writefile append loop loop_exit sleep_test\n");
}

// 用户态 shell 主循环：保持最小交互式行为即可。
void _start(void) {
    static char line[SHELL_LINE_MAX];
    static char* argv[SHELL_ARGV_MAX];
    static char* hello_argv[] = {"hello"};

    user_write("shell start\n");

    for (;;) {
        int argc;

        user_write("MiniOS$ ");
        shell_read_line(line, SHELL_LINE_MAX);
        argc = shell_split_line(line, argv, SHELL_ARGV_MAX);
        if (argc < 0) {
            user_write("Too many args\n");
            continue;
        }
        if (argc == 0) {
            continue;
        }

        if (shell_streq(argv[0], "help")) {
            shell_cmd_help();
            continue;
        }

        if (shell_streq(argv[0], "echo")) {
            shell_cmd_echo(argc, argv);
            continue;
        }

        if (shell_streq(argv[0], "clear")) {
            // clear 只负责请求内核清屏；下一轮循环会自然重新打印提示符。
            user_clear_screen();
            continue;
        }

        if (shell_streq(argv[0], "ps")) {
            shell_cmd_ps();
            continue;
        }

        if (shell_streq(argv[0], "jobs")) {
            shell_cmd_jobs();
            continue;
        }

        if (shell_streq(argv[0], "ls")) {
            shell_cmd_ls(argc);
            continue;
        }

        if (shell_streq(argv[0], "cat")) {
            shell_cmd_cat(argc, argv);
            continue;
        }

        if (shell_streq(argv[0], "touch")) {
            shell_cmd_touch(argc, argv);
            continue;
        }

        if (shell_streq(argv[0], "writefile")) {
            shell_cmd_writefile(argc, argv);
            continue;
        }

        if (shell_streq(argv[0], "append")) {
            shell_cmd_append(argc, argv);
            continue;
        }

        if (shell_streq(argv[0], "rm")) {
            shell_cmd_rm(argc, argv);
            continue;
        }

        if (shell_streq(argv[0], "fdtest")) {
            shell_cmd_fdtest();
            continue;
        }

        if (shell_streq(argv[0], "uptime") || shell_streq(argv[0], "ticks")) {
            shell_cmd_uptime();
            continue;
        }

        if (shell_streq(argv[0], "sleep")) {
            shell_cmd_sleep(argc, argv);
            continue;
        }

        if (shell_streq(argv[0], "wait")) {
            int pid = 0;
            int wait_result;

            if (argc == 1) {
                wait_result = user_wait_any();
                if (wait_result <= 0) {
                    user_write("No exited child\n");
                    continue;
                }

                user_write("Wait done pid: ");
                shell_write_uint(wait_result);
                user_write("\n");
                continue;
            }

            if (argc != 2) {
                user_write("Usage: wait [pid]\n");
                continue;
            }

            if (shell_atoi(argv[1], &pid) == 0) {
                user_write("Invalid pid\n");
                continue;
            }

            wait_result = user_waitpid(pid);
            if (wait_result < 0) {
                user_write("Wait failed\n");
                continue;
            }

            user_write("Wait done pid: ");
            shell_write_uint(wait_result);
            user_write("\n");
            continue;
        }

        if (shell_streq(argv[0], "kill")) {
            int pid = 0;

            if (argc <= 1) {
                user_write("Usage: kill <pid>\n");
                continue;
            }

            if (shell_atoi(argv[1], &pid) == 0) {
                user_write("Invalid pid\n");
                continue;
            }

            if (user_kill(pid) < 0) {
                user_write("Kill failed\n");
                continue;
            }

            user_write("Killed\n");
            continue;
        }

        if (shell_streq(argv[0], "run") || shell_streq(argv[0], "start")) {
            int is_start = shell_streq(argv[0], "start");
            int program_id;
            int validate_result;

            if (argc <= 1) {
                if (is_start != 0) {
                    user_write("Usage: start <program>\n");
                } else {
                    user_write("Usage: run <program>\n");
                }
                continue;
            }

            program_id = shell_program_id_from_name(argv[1]);
            if (program_id == PROGRAM_INVALID) {
                user_write("Unknown program\n");
                continue;
            }

            validate_result = shell_validate_program_args(argc - 1, &argv[1]);
            if (validate_result == -1) {
                user_write("Too many args\n");
                continue;
            }
            if (validate_result == -2) {
                user_write("Arg too long\n");
                continue;
            }

            shell_spawn_program(program_id, argc - 1, &argv[1], is_start == 0, is_start != 0);
            continue;
        }

        if (shell_streq(argv[0], "hello")) {
            shell_spawn_program(PROGRAM_HELLO, 1, hello_argv, 1, 0);
            continue;
        }

        if (shell_streq(argv[0], "exit")) {
            user_write("shell exit\n");
            user_exit(0);
        }

        user_write("Unknown command\n");
    }
}
