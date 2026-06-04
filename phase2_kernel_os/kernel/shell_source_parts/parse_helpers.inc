/* ===== shell 通用字符串/语法辅助 ===== */

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

// 判断一个 token 是否是当前教学版 shell 支持的输出重定向符号。
static int shell_is_redirect_token(const char* token) {
    if (shell_streq(token, ">") != 0) {
        return 1;
    }

    if (shell_streq(token, ">>") != 0) {
        return 1;
    }

    return 0;
}

// 判断一个 token 是否是当前教学版 shell 支持的输入重定向符号。
static int shell_is_input_redirect_token(const char* token) {
    return shell_streq(token, "<");
}

// 判断一个 token 是否是教学版单管道符号。
static int shell_is_pipe_token(const char* token) {
    return shell_streq(token, "|");
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

// 把 [start_index, end_index) 范围内的参数用单个空格拼成一段文本；供 echo 重定向提取输出内容复用。
static int shell_join_arg_range(int argc, char** argv, int start_index, int end_index, char* buffer, int max_len) {
    int i;
    int out = 0;

    if (buffer == (char*)0 || max_len <= 0) {
        return -1;
    }

    if (start_index < 0) {
        start_index = 0;
    }
    if (end_index > argc) {
        end_index = argc;
    }
    if (start_index > end_index) {
        start_index = end_index;
    }

    buffer[0] = '\0';
    for (i = start_index; i < end_index; i++) {
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

// 扫描当前命令是否包含 > 或 >>；如果找到则返回其位置，并说明是否为追加写。
static int shell_find_redirect(int argc, char** argv, int* out_index, int* out_is_append) {
    int i;
    int found_index = -1;
    int found_is_append = 0;

    if (out_index == (int*)0 || out_is_append == (int*)0) {
        return -1;
    }

    for (i = 0; i < argc; i++) {
        if (shell_is_redirect_token(argv[i]) == 0) {
            continue;
        }

        if (found_index >= 0) {
            return -2;
        }

        found_index = i;
        found_is_append = shell_streq(argv[i], ">>");
    }

    *out_index = found_index;
    *out_is_append = found_is_append;
    return 0;
}

// 扫描当前命令是否包含 <；如果找到则返回其位置。
static int shell_find_input_redirect(int argc, char** argv, int* out_index) {
    int i;
    int found_index = -1;

    if (out_index == (int*)0) {
        return -1;
    }

    for (i = 0; i < argc; i++) {
        if (shell_is_input_redirect_token(argv[i]) == 0) {
            continue;
        }

        if (found_index >= 0) {
            return -2;
        }

        found_index = i;
    }

    *out_index = found_index;
    return 0;
}

// 扫描当前命令是否包含单个 |；当前只支持一段教学版单管道。
static int shell_find_pipe(int argc, char** argv, int* out_index) {
    int i;
    int found_index = -1;

    if (out_index == (int*)0) {
        return -1;
    }

    for (i = 0; i < argc; i++) {
        if (shell_is_pipe_token(argv[i]) == 0) {
            continue;
        }

        if (found_index >= 0) {
            return -2;
        }

        found_index = i;
    }

    *out_index = found_index;
    return 0;
}

// 解析 run 命令中的教学版 < / > / >> 重定向；当前只支持每种最多一个，并保证它们不会进入用户 argv。
static int shell_parse_run_redirects(int argc, char** argv, int output_index, int output_is_append, int input_index, struct shell_redirect_info* out_info) {
    int i;
    int seen_input = 0;
    int seen_output = 0;

    if (out_info == (struct shell_redirect_info*)0) {
        return -1;
    }

    out_info->has_stdin = (input_index >= 0);
    out_info->stdin_index = input_index;
    out_info->stdin_path = (const char*)0;
    out_info->has_stdout = (output_index >= 0);
    out_info->stdout_index = output_index;
    out_info->stdout_append = output_is_append;
    out_info->stdout_path = (const char*)0;
    out_info->first_redirect_index = -1;

    if (out_info->has_stdin == 0 && out_info->has_stdout == 0) {
        return 0;
    }

    if (out_info->has_stdin != 0 && out_info->has_stdout != 0) {
        out_info->first_redirect_index = (input_index < output_index) ? input_index : output_index;
    } else if (out_info->has_stdin != 0) {
        out_info->first_redirect_index = input_index;
    } else {
        out_info->first_redirect_index = output_index;
    }

    for (i = out_info->first_redirect_index; i < argc;) {
        if (shell_is_input_redirect_token(argv[i]) != 0) {
            if (out_info->has_stdin == 0 || i != out_info->stdin_index || seen_input != 0) {
                return -2;
            }
            if ((i + 1) >= argc) {
                return -3;
            }
            if (shell_is_input_redirect_token(argv[i + 1]) != 0 || shell_is_redirect_token(argv[i + 1]) != 0) {
                return -3;
            }

            out_info->stdin_path = argv[i + 1];
            seen_input = 1;
            i += 2;
            continue;
        }

        if (shell_is_redirect_token(argv[i]) != 0) {
            if (out_info->has_stdout == 0 || i != out_info->stdout_index || seen_output != 0) {
                return -4;
            }
            if ((i + 1) >= argc) {
                return -5;
            }
            if (shell_is_input_redirect_token(argv[i + 1]) != 0 || shell_is_redirect_token(argv[i + 1]) != 0) {
                return -5;
            }

            out_info->stdout_path = argv[i + 1];
            seen_output = 1;
            i += 2;
            continue;
        }

        return -6;
    }

    if (out_info->has_stdin != 0 && seen_input == 0) {
        return -7;
    }
    if (out_info->has_stdout != 0 && seen_output == 0) {
        return -8;
    }

    return 0;
}

// 根据 run 命令解析出的重定向信息，计算真正应该传给用户程序的 argc。
// 这样可以统一保证 < / > / >> 以及它们的目标文件名不会混进用户程序 argv。
static int shell_effective_run_argc(int argc, const struct shell_redirect_info* info) {
    if (info == (const struct shell_redirect_info*)0) {
        return argc;
    }

    if (info->first_redirect_index > 0 && info->first_redirect_index < argc) {
        return info->first_redirect_index;
    }

    return argc;
}
