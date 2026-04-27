#ifndef MINIOS_FS_H
#define MINIOS_FS_H

#include <string>
#include <vector>

struct FileNode {
    // 文件名（单目录模型下唯一标识）。
    std::string name;
    // 文件内容（本轮仅支持完整覆盖写）。
    std::string content;
};

class FileSystem {
public:
    // 创建空文件；若已存在则直接返回成功。
    bool touch(const std::string& name);
    // 覆盖写文件内容；若文件不存在则自动创建。
    bool write(const std::string& name, const std::string& text);
    // 输出文件内容到标准输出；不存在时输出错误提示。
    bool cat(const std::string& name) const;
    // 输出单个文件元信息（文件名与内容长度）。
    bool stat(const std::string& name) const;
    // 删除指定文件；不存在时输出错误提示。
    bool remove(const std::string& name);
    // 列出当前 MiniFS 中的所有文件名。
    void ls() const;

private:
    // 单目录平面文件表。
    std::vector<FileNode> files;
};

#endif
