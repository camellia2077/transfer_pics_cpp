#ifndef IRENDERER_H
#define IRENDERER_H

#include "common_types.h"
#include <filesystem>
#include <string>
#include <vector>

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // 纯虚函数，用于执行渲染操作
    virtual bool render(
        const std::vector<std::vector<CharColorInfo>>& asciiData,
        const std::filesystem::path& outputPath,
        const Config& config,
        ColorScheme scheme) const = 0;

    // 纯虚函数，用于获取该渲染器对应的文件扩展名
    virtual std::string getOutputFileExtension() const = 0;
};

#endif // IRENDERER_H