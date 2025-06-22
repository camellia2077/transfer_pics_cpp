// config_handler.h
#ifndef CONFIG_HANDLER_H
#define CONFIG_HANDLER_H

#include "common_types.h" // Includes filesystem, string, vector, etc.
#include <filesystem>

// 从指定的 JSON 文件路径加载配置到 Config 结构体中。
// 如果文件不存在或解析失败，则使用默认值。
// 成功加载返回 true，否则返回 false。
bool loadConfiguration(const std::filesystem::path& configPath, Config& config);

// 将当前生效的配置写入一个文本文件，用于记录和调试。
// 成功返回 true，失败返回 false。
bool writeConfigToFile(const Config& config, const std::filesystem::path& outputFilePath);

// helpers 的函数声明
string colorSchemeToString(ColorScheme scheme);
string getSchemeSuffix(ColorScheme scheme);
const std::unordered_map<std::string, ColorScheme>& getColorSchemeMap();


#endif // CONFIG_HANDLER_H
