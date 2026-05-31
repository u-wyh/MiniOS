/* ===== run 重定向/pipe 的校验与错误输出 ===== */

// 输出 run 教学版重定向解析错误：尽量区分“缺少输入/输出目标”和一般语法错误，便于用户定位问题。
static void shell_write_run_redirect_error(int parse_result) {
    if (parse_result == -3 || parse_result == -7) {
        user_write("redirect: missing input file\n");
        return;
    }

    if (parse_result == -5 || parse_result == -8) {
        user_write("redirect: missing target file\n");
        return;
    }

    user_write("redirect: invalid syntax\n");
}

// 校验“stdout -> 文件”目标是否允许写入：只读内置文件直接拒绝，>> 仍要求目标已存在且必须是 RAMFS。
static int shell_validate_stdout_target(const char* path, int is_append) {
    struct minios_stat st;
    int stat_result;

    if (path == (const char*)0 || path[0] == '\0') {
        return -1;
    }

    stat_result = user_stat(path, &st);
    if (stat_result == 0) {
        if (st.type != MINIOS_FILE_TYPE_RAMFS_TEXT) {
            return -2;
        }
        return 0;
    }

    if (is_append != 0) {
        return -3;
    }

    return 0;
}

// 校验“stdin <- 文件”源是否可读：当前只要求路径存在，允许内置只读文件和 RAMFS 文件都作为输入源。
static int shell_validate_stdin_source(const char* path) {
    struct minios_stat st;

    if (path == (const char*)0 || path[0] == '\0') {
        return -1;
    }

    if (user_stat(path, &st) < 0) {
        return -2;
    }

    return 0;
}

// 执行 echo 的教学版 RAMFS 输出重定向：> 覆盖写，必要时自动创建；>> 追加写且要求目标已存在。
static void shell_cmd_echo_redirect(int argc, char** argv, int redirect_index, int is_append) {
    char text[SHELL_WRITEFILE_MAX_LEN];
    int result;

    if (redirect_index <= 0) {
        user_write("redirect: invalid syntax\n");
        return;
    }

    if ((redirect_index + 1) >= argc) {
        user_write("redirect: missing target file\n");
        return;
    }

    if ((redirect_index + 2) != argc) {
        user_write("redirect: only one target file supported\n");
        return;
    }

    if (shell_join_arg_range(argc, argv, 1, redirect_index, text, SHELL_WRITEFILE_MAX_LEN) < 0) {
        user_write("redirect: text too long\n");
        return;
    }

    if (is_append != 0) {
        result = user_appendfile(argv[redirect_index + 1], text);
        if (result < 0) {
            user_write("redirect append failed\n");
        }
        return;
    }

    result = user_writefile(argv[redirect_index + 1], text);
    if (result == -3) {
        if (user_touch(argv[redirect_index + 1]) >= 0) {
            result = user_writefile(argv[redirect_index + 1], text);
        }
    }

    if (result < 0) {
        user_write("redirect write failed\n");
    }
}

