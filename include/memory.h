#ifndef MINIOS_MEMORY_H
#define MINIOS_MEMORY_H

#include <cstddef>
#include <string>
#include <vector>

struct MemoryBlock {
    int id;
    std::size_t size;
    bool used;
};

class MemoryManager {
public:
    MemoryManager();

    int alloc(std::size_t size);
    bool freeBlock(int id);
    void stat() const;

private:
    std::size_t totalSize;
    std::vector<MemoryBlock> blocks;
    int nextBlockId;
};

// 处理 mem 命令：alloc/free/stat。
bool executeMemoryCommand(const std::vector<std::string>& tokens);

#endif
