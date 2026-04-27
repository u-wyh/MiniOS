#include "fs.h"

#include <algorithm>
#include <iomanip>
#include <iostream>

bool FileSystem::touch(const std::string& name) {
    // 创建空文件：名称非法返回失败；已存在则保持成功。
    if (name.empty()) {
        return false;
    }

    // 简化版 touch：已存在时视为成功，不修改内容。
    const auto it = std::find_if(files.begin(), files.end(), [&](const FileNode& file) {
        return file.name == name;
    });
    if (it != files.end()) {
        return true;
    }

    files.push_back(FileNode{name, ""});
    return true;
}

bool FileSystem::write(const std::string& name, const std::string& text) {
    // 写文件：名称非法返回失败；不存在则自动创建，存在则覆盖写。
    if (name.empty()) {
        return false;
    }

    auto it = std::find_if(files.begin(), files.end(), [&](const FileNode& file) {
        return file.name == name;
    });

    // 若文件不存在则自动创建，符合教学版 MiniFS 的使用体验。
    if (it == files.end()) {
        files.push_back(FileNode{name, text});
        return true;
    }

    // 本轮采用覆盖写。
    it->content = text;
    return true;
}

bool FileSystem::cat(const std::string& name) const {
    // 读文件：找到即输出内容，未找到输出统一错误提示。
    const auto it = std::find_if(files.begin(), files.end(), [&](const FileNode& file) {
        return file.name == name;
    });

    if (it == files.end()) {
        std::cout << "File not found\n";
        return false;
    }

    std::cout << it->content << '\n';
    return true;
}

bool FileSystem::stat(const std::string& name) const {
    // 查看文件元信息：输出名称和内容长度，不存在则提示错误。
    const auto it = std::find_if(files.begin(), files.end(), [&](const FileNode& file) {
        return file.name == name;
    });

    if (it == files.end()) {
        std::cout << "File not found\n";
        return false;
    }

    std::cout << std::left
              << std::setw(20) << "NAME"
              << std::setw(10) << "SIZE"
              << '\n';
    std::cout << std::setw(20) << it->name
              << std::setw(10) << it->content.size()
              << '\n';
    return true;
}

bool FileSystem::remove(const std::string& name) {
    // 删除文件：存在则删除，缺失则提示并返回失败。
    const auto it = std::find_if(files.begin(), files.end(), [&](const FileNode& file) {
        return file.name == name;
    });

    if (it == files.end()) {
        std::cout << "File not found\n";
        return false;
    }

    files.erase(it);
    return true;
}

void FileSystem::ls() const {
    // 列目录：按文件名排序后逐行输出，保证显示稳定。
    // 按文件名输出，便于结果稳定可读。
    std::vector<std::string> names;
    names.reserve(files.size());
    for (const auto& file : files) {
        names.push_back(file.name);
    }

    std::sort(names.begin(), names.end());
    std::cout << std::left
              << std::setw(20) << "NAME"
              << std::setw(10) << "SIZE"
              << '\n';
    for (const auto& name : names) {
        const auto it = std::find_if(files.begin(), files.end(), [&](const FileNode& file) {
            return file.name == name;
        });
        const std::size_t size = (it == files.end()) ? 0 : it->content.size();
        std::cout << std::setw(20) << name
                  << std::setw(10) << size
                  << '\n';
    }
}
