/* ===== shell 内建命令实现 ===== */

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
    user_write("  <program> [args]\n");
    user_write("  <program> [args] > <file>\n");
    user_write("  <program> [args] >> <file>\n");
    user_write("  <program> [args] < <file>\n");
    user_write("  <program> [args] < <file> > <file>\n");
    user_write("  <program> [args] | <program> [args]\n");
    user_write("  <program> [args] < <file> | <program> [args]\n");
    user_write("  <program> [args] < <file> | <program> [args] > <file>\n");
    user_write("  <program> [args] < <file> | <program> [args] >> <file>\n");
    user_write("  <program> [args] | <program> [args] > <file>\n");
    user_write("  <program> [args] | <program> [args] >> <file>\n");
    user_write("  start <program> [args]\n");
    user_write("  jobs\n");
    user_write("  wait [pid]\n");
    user_write("  kill <pid>\n");
    user_write("  touch <file>\n");
    user_write("  rm <file>\n");
    user_write("  ps    show process table with age/runs\n");
    user_write("  uptime\n");
    user_write("  ticks\n");
    user_write("  sleep <ticks>\n");
    user_write("  exit\n");
    user_write("programs:\n");
    user_write("  hello echo ls cat stat writefile append wc grep head tail sort\n");
    user_write("  loop loop_exit sleep_test pipe_test dup2_test fork_fd_test\n");
    user_write("  pipe_fork_dup2_test pipe_close_test exec_fd_test exec_args_test\n");
    user_write("  pipeline_demo pipeline_args_demo mini_pipeline pipe_multi_test\n");
    user_write("compat:\n");
    user_write("  run <program> [args]   (legacy alias, no longer required)\n");
}
