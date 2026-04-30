#ifndef ELF_H
#define ELF_H

// ELF 魔数与最小格式常量（仅支持 32 位小端）
#define ELF_MAGIC 0x464C457F
#define ELFCLASS32 1
#define ELFDATA2LSB 1

// Program Header 类型：可加载段
#define PT_LOAD 1

// 段权限位
#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

// ELF 文件头：本轮仅使用入口地址与 Program Header 表信息
struct Elf32_Ehdr {
    unsigned char e_ident[16];
    unsigned short e_type;
    unsigned short e_machine;
    unsigned int e_version;
    unsigned int e_entry;
    unsigned int e_phoff;
    unsigned int e_shoff;
    unsigned int e_flags;
    unsigned short e_ehsize;
    unsigned short e_phentsize;
    unsigned short e_phnum;
    unsigned short e_shentsize;
    unsigned short e_shnum;
    unsigned short e_shstrndx;
};

// Program Header：描述一个段如何从文件装载到内存
struct Elf32_Phdr {
    unsigned int p_type;
    unsigned int p_offset;
    unsigned int p_vaddr;
    unsigned int p_paddr;
    unsigned int p_filesz;
    unsigned int p_memsz;
    unsigned int p_flags;
    unsigned int p_align;
};

// 从内存中的 ELF 镜像加载用户程序，成功返回入口虚拟地址，失败返回 0
unsigned int elf_load(const unsigned char* elf_data, unsigned int elf_size);

#endif
