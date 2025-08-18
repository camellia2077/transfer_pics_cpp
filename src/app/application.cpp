// src/app/application.cpp

#include "application.h"
#include "config_handler.h"
#include "ui/cli_handler.h"
#include "utils/PathManager.h"
#include "core/processing_orchestrator.h"

#include <iostream>
#include <chrono>

using namespace std::chrono;

Application::Application(int argc, char* argv[]) : m_argc(argc), m_argv(argv) {}

int Application::run() {
    CLIHandler::printWelcomeMessage();

    if (!initialize()) {
        return 1; // 初始化失败，直接退出
    }

    // --- 核心修改：处理命令行参数 ---
    if (m_argc < 2) {
        std::filesystem::path programPath(m_argv[0]);
        CLIHandler::printUsage(programPath.filename().string());
        return 1; // 参数不足，打印用法并退出
    }

    std::string inputPathStr = m_argv[1];
    
    // 可选：处理 --help 或 -h 参数
    if (inputPathStr == "--help" || inputPathStr == "-h") {
        std::filesystem::path programPath(m_argv[0]);
        CLIHandler::printUsage(programPath.filename().string());
        return 0; // 打印用法后正常退出
    }
    // --- 结束核心修改 ---


    CLIHandler::printEffectiveConfiguration(m_config);

    auto overall_start_time = high_resolution_clock::now();

    ProcessingOrchestrator orchestrator(m_config);
    orchestrator.process(inputPathStr); // 使用从命令行获取的路径

    auto overall_end_time = high_resolution_clock::now();
    double total_duration = duration_cast<duration<double>>(overall_end_time - overall_start_time).count();

    CLIHandler::printProcessingSummary(
        orchestrator.getProcessedCount(),
        orchestrator.getFailedCount(),
        total_duration,
        orchestrator.getFinalOutputDir()
    );

    // 如果有任何文件处理失败，返回一个非零的退出码
    return (orchestrator.getFailedCount() > 0) ? 1 : 0;
}

bool Application::initialize() {
    std::filesystem::path exePath = PathManager::getExecutablePath(m_argc, m_argv);
    m_exeDir = exePath.parent_path();

    const std::string configFilename = "config.json";
    std::filesystem::path configPathObj = m_exeDir / configFilename;

    if (!loadConfiguration(configPathObj, m_config)) {
        std::cout << "Error: Configuration file could not be parsed correctly. Please check config.json. Proceeding with default values." << std::endl;
    }
    
    return resolveFontPath();
}

bool Application::resolveFontPath() {
    std::filesystem::path potentialFontPath = m_exeDir / m_config.fontFilename;
    if (std::filesystem::exists(potentialFontPath) && std::filesystem::is_regular_file(potentialFontPath)) {
        m_config.finalFontPath = potentialFontPath.string();
    } else {
        potentialFontPath = std::filesystem::current_path() / m_config.fontFilename;
        if (std::filesystem::exists(potentialFontPath) && std::filesystem::is_regular_file(potentialFontPath)) {
            m_config.finalFontPath = potentialFontPath.string();
            std::cout << "Info: Font found in current working directory: " << m_config.finalFontPath << std::endl;
        } else {
            std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
            std::cerr << "Error: Font file '" << m_config.fontFilename << "' not found!" << std::endl;
            std::cerr << "Searched near executable: " << (m_exeDir / m_config.fontFilename).string() << std::endl;
            std::cerr << "Searched in current dir: " << (std::filesystem::current_path() / m_config.fontFilename).string() << std::endl;
            std::cerr << "Please ensure '" << m_config.fontFilename << "' is placed correctly or update config.json." << std::endl;
            std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
            return false;
        }
    }
    return true;
}