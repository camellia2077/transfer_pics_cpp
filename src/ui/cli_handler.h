// src/ui/cli_handler.h

#ifndef CLI_HANDLER_H
#define CLI_HANDLER_H

#include "common_types.h"
#include <string>
#include <filesystem>

namespace CLIHandler {

    void printWelcomeMessage();
    
    // 新增：打印使用说明
    void printUsage(const std::string& programName);

    void printEffectiveConfiguration(const Config& config);
    void printProcessingSummary(int processedCount, int failedCount, double duration, const std::filesystem::path& outputDir);

} // namespace CLIHandler

#endif // CLI_HANDLER_H