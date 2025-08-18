#ifndef PROCESSING_ORCHESTRATOR_H
#define PROCESSING_ORCHESTRATOR_H

#include "common/common_types.h"
#include "rendering/IRenderer.h"
#include <filesystem>
#include <vector>
#include <memory>

class ProcessingOrchestrator {
public:
    ProcessingOrchestrator(const Config& config);
    void process(const std::filesystem::path& inputPath);

    int getProcessedCount() const { return m_processedCount; }
    int getFailedCount() const { return m_failedCount; }
    const std::filesystem::path& getFinalOutputDir() const { return m_finalMainOutputDirPath; }

private:
    void setupRenderers();
    void processSingleImage(const std::filesystem::path& imagePath);
    void processDirectory(const std::filesystem::path& dirPath);
    bool processImageFile(const std::filesystem::path& imagePath, const std::filesystem::path& outputSubDirPath);

    const Config& m_config;
    int m_processedCount = 0;
    int m_failedCount = 0;
    std::filesystem::path m_finalMainOutputDirPath;
    std::vector<std::unique_ptr<IRenderer>> m_renderers;
};

#endif // PROCESSING_ORCHESTRATOR_H