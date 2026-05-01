#include "elf.h"
#include "exec.h"
#include "fs.h"
#include "user.h"
#include "vga.h"

#define USER_STACK_TOP 0x00800000

// 汇编入口：通过 iret 切换到用户态并从指定入口开始执行
extern void enter_user_mode(unsigned int user_entry, unsigned int user_stack_top);

// 计算 ELF 镜像占用的最小字节长度，供 elf_load 传参
static unsigned int exec_get_elf_size(const unsigned char* elf_data) {
    const struct Elf32_Ehdr* ehdr = (const struct Elf32_Ehdr*)elf_data;
    const struct Elf32_Phdr* phdr;
    unsigned int end;
    unsigned int i;

    end = ehdr->e_phoff + ((unsigned int)ehdr->e_phnum * sizeof(struct Elf32_Phdr));
    phdr = (const struct Elf32_Phdr*)(elf_data + ehdr->e_phoff);

    for (i = 0; i < ehdr->e_phnum; i++) {
        unsigned int seg_end;

        if (phdr[i].p_type != PT_LOAD) {
            continue;
        }

        seg_end = phdr[i].p_offset + phdr[i].p_filesz;
        if (seg_end > end) {
            end = seg_end;
        }
    }

    return end;
}

// 按文件名执行用户程序：通过 fs_find 查找内存文件并交给 ELF loader
void exec(const char* name) {
    struct file* target;
    unsigned int elf_size;
    unsigned int entry;

    if (name == (const char*)0 || name[0] == '\0') {
        print_string("exec: empty name\n");
        return;
    }

    target = fs_find(name);
    if (target == (struct file*)0) {
        print_string("exec: file not found\n");
        return;
    }

    // 复用现有用户空间初始化，确保用户栈和基础映射已经准备好
    user_space_init();

    // 先按文件记录大小加载；若记录值异常，再回退到扫描计算
    elf_size = (unsigned int)target->size;
    if (elf_size == 0) {
        elf_size = exec_get_elf_size((const unsigned char*)target->data);
    }

    entry = elf_load((const unsigned char*)target->data, elf_size);
    if (entry == 0) {
        print_string("exec: elf load failed\n");
        return;
    }

    // 最小 exec 流程：进入用户态执行 ELF 入口
    enter_user_mode(entry, USER_STACK_TOP);
}
