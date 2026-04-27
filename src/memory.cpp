#include "memory.h"

#include <iostream>
#include <string>

// 初始化逻辑内存池：总容量 1024 bytes，块 id 从 1 开始递增。
MemoryManager::MemoryManager() : totalSize(1024), nextBlockId(1) {}

int MemoryManager::alloc(std::size_t size) {
    // 0 字节分配直接视为非法请求。
    if (size == 0) {
        return -1;
    }

    // 优先复用可用且容量足够的空闲块，减少逻辑碎片。
    for (auto& block : blocks) {
        if (!block.used && block.size >= size) {
            block.used = true;
            return block.id;
        }
    }

    std::size_t usedSize = 0;
    for (const auto& block : blocks) {
        if (block.used) {
            usedSize += block.size;
        }
    }

    if (usedSize + size > totalSize) {
        return -1;
    }

    // 无可复用空闲块时，新建逻辑块并标记为已使用。
    MemoryBlock block{};
    block.id = nextBlockId++;
    block.size = size;
    block.used = true;
    blocks.push_back(block);
    return block.id;
}

bool MemoryManager::freeBlock(int id) {
    // 仅允许释放存在且当前处于 Used 的块。
    for (auto& block : blocks) {
        if (block.id == id) {
            if (!block.used) {
                return false;
            }
            block.used = false;
            return true;
        }
    }
    return false;
}

void MemoryManager::stat() const {
    // 统计口径：Used 为所有 used block 的 size 之和，Free = Total - Used。
    std::size_t usedSize = 0;
    for (const auto& block : blocks) {
        if (block.used) {
            usedSize += block.size;
        }
    }

    std::cout << "Total: " << totalSize << '\n';
    std::cout << "Used : " << usedSize << '\n';
    std::cout << "Free : " << (totalSize - usedSize) << '\n';
    std::cout << "\nBlocks:\n";
    std::cout << "ID   SIZE   STATE\n";
    for (const auto& block : blocks) {
        std::cout << block.id << "    " << block.size << "    " << (block.used ? "Used" : "Free") << '\n';
    }
}

namespace {

// 进程内单例内存管理器，供 mem 命令统一复用。
MemoryManager g_memoryManager;

}  // namespace

bool executeMemoryCommand(const std::vector<std::string>& tokens) {
    // 仅处理 mem 前缀，其余命令交回上层分发。
    if (tokens.empty() || tokens[0] != "mem") {
        return false;
    }

    if (tokens.size() < 2) {
        std::cout << "Usage: mem <alloc|free|stat>\n";
        return true;
    }

    const std::string& action = tokens[1];
    if (action == "alloc") {
        if (tokens.size() != 3) {
            std::cout << "Usage: mem alloc <size>\n";
            return true;
        }

        long long size = 0;
        try {
            size = std::stoll(tokens[2]);
        } catch (...) {
            std::cout << "Invalid allocation size\n";
            return true;
        }

        if (size <= 0) {
            std::cout << "Invalid allocation size\n";
            return true;
        }

        // 分配失败统一表示容量不足（或无法满足请求）。
        int id = g_memoryManager.alloc(static_cast<std::size_t>(size));
        if (id < 0) {
            std::cout << "Allocation failed: not enough memory\n";
            return true;
        }
        std::cout << "allocated block id=" << id << " size=" << size << '\n';
        return true;
    }

    if (action == "free") {
        if (tokens.size() != 3) {
            std::cout << "Usage: mem free <id>\n";
            return true;
        }

        int id = 0;
        try {
            id = std::stoi(tokens[2]);
        } catch (...) {
            std::cout << "Invalid block id\n";
            return true;
        }

        if (!g_memoryManager.freeBlock(id)) {
            std::cout << "Invalid block id\n";
            return true;
        }
        std::cout << "freed block id=" << id << '\n';
        return true;
    }

    if (action == "stat") {
        if (tokens.size() != 2) {
            std::cout << "Usage: mem stat\n";
            return true;
        }
        g_memoryManager.stat();
        return true;
    }

    std::cout << "Usage: mem <alloc|free|stat>\n";
    return true;
}
