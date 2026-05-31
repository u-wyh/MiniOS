/* ===== shell 内建命令实现 ===== */

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
    user_write("  echo <text> > <file>\n");
    user_write("  echo <text> >> <file>\n");
    user_write("  run <program> [args]\n");
    user_write("  run <program> [args] > <file>\n");
    user_write("  run <program> [args] >> <file>\n");
    user_write("  run cat < <file>\n");
    user_write("  run cat < <file> > <file>\n");
    user_write("  run <program> [args] | run <program> [args]\n");
    user_write("  run <program> < <file> | run <program> [args]\n");
    user_write("  run <program> < <file> | run <program> [args] > <file>\n");
    user_write("  run <program> < <file> | run <program> [args] >> <file>\n");
    user_write("  run <program> [args] | run <program> [args] > <file>\n");
    user_write("  run <program> [args] | run <program> [args] >> <file>\n");
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

