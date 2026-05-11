// user_program.c：统一维护 program_id、程序名与内置镜像查询关系
#include "fs.h"
#include "user_program.h"

// 裸机环境下最小字符串比较：供名字查找统一复用。
static int user_program_name_equals(const char* left, const char* right) {
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

// 统一的用户程序描述表：program_id/name/shell 可见性只维护一份。
static struct user_program_descriptor user_program_table[] = {
#define MINIOS_BUILD_PROGRAM_DESC(symbol, value, program_name, visible) \
    {symbol, program_name, (const unsigned char*)0, 0, visible},
    MINIOS_USER_PROGRAM_LIST(MINIOS_BUILD_PROGRAM_DESC)
#undef MINIOS_BUILD_PROGRAM_DESC
};

// 统计当前统一程序表项数，避免手写魔法常量。
static uint32_t user_program_count(void) {
    return (uint32_t)(sizeof(user_program_table) / sizeof(user_program_table[0]));
}

// 按需把 program_id 对应的镜像地址/大小从当前内置 ramfs 取回，避免多维护一份 blob 表。
static void user_program_resolve_image(struct user_program_descriptor* descriptor) {
    struct file* file;

    if (descriptor == (struct user_program_descriptor*)0) {
        return;
    }

    if (descriptor->image != (const unsigned char*)0 && descriptor->image_size != 0) {
        return;
    }

    file = fs_find(descriptor->name);
    if (file == (struct file*)0) {
        descriptor->image = (const unsigned char*)0;
        descriptor->image_size = 0;
        return;
    }

    descriptor->image = (const unsigned char*)file->data;
    descriptor->image_size = file->size;
}

// 按编号查找描述符，并补齐镜像地址/大小。
const struct user_program_descriptor* user_program_get_by_id(int program_id) {
    uint32_t i;

    for (i = 0; i < user_program_count(); i++) {
        if (user_program_table[i].program_id != program_id) {
            continue;
        }

        user_program_resolve_image(&user_program_table[i]);
        return &user_program_table[i];
    }

    return (const struct user_program_descriptor*)0;
}

// 按规范程序名查找描述符，供 shell 和 exec 语义统一复用。
const struct user_program_descriptor* user_program_find_by_name(const char* name) {
    uint32_t i;

    if (name == (const char*)0 || name[0] == '\0') {
        return (const struct user_program_descriptor*)0;
    }

    for (i = 0; i < user_program_count(); i++) {
        if (user_program_name_equals(name, user_program_table[i].name) == 0) {
            continue;
        }

        user_program_resolve_image(&user_program_table[i]);
        return &user_program_table[i];
    }

    return (const struct user_program_descriptor*)0;
}

// 返回规范程序名，便于日志和文档共用同一套命名。
const char* user_program_name(int program_id) {
    const struct user_program_descriptor* descriptor = user_program_get_by_id(program_id);

    if (descriptor == (const struct user_program_descriptor*)0) {
        return (const char*)0;
    }

    return descriptor->name;
}

// 判断 program_id 是否有效。
int user_program_is_valid(int program_id) {
    return user_program_get_by_id(program_id) != (const struct user_program_descriptor*)0;
}

// 判断该程序是否暴露给用户态 shell 的 run/start 命令。
int user_program_is_shell_runnable(int program_id) {
    const struct user_program_descriptor* descriptor = user_program_get_by_id(program_id);

    if (descriptor == (const struct user_program_descriptor*)0) {
        return 0;
    }

    return descriptor->shell_visible;
}
