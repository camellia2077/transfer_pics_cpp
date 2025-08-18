#include "PathManager.h"
#include <iostream>
#include <system_error> // For std::error_code

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

namespace PathManager {

std::filesystem::path getExecutablePath(int argc, char* argv[]) {
    std::filesystem::path exePath;
    try {
        if (argc > 0 && argv[0] != nullptr) {
            std::error_code ec;
            std::filesystem::path tempPath = std::filesystem::canonical(argv[0], ec);
            if (!ec) {
                exePath = tempPath;
            } else {
                #ifdef _WIN32
                    char pathBuf[MAX_PATH];
                    if (GetModuleFileNameA(NULL, pathBuf, MAX_PATH) != 0) {
                        exePath = pathBuf;
                    } else {
                        exePath = std::filesystem::current_path() / "ascii_generator_fallback.exe";
                    }
                #else
                    exePath = std::filesystem::current_path() / "ascii_generator_fallback";
                #endif
            }
        } else {
            #ifdef _WIN32
                char pathBuf[MAX_PATH];
                if (GetModuleFileNameA(NULL, pathBuf, MAX_PATH) != 0) {
                    exePath = pathBuf;
                } else {
                    exePath = std::filesystem::current_path() / "ascii_generator_fallback.exe";
                }
            #else
                exePath = std::filesystem::current_path() / "ascii_generator_fallback";
            #endif
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: Exception resolving executable path: " << e.what() << ". Using fallback." << std::endl;
        std::string fallbackName = "ascii_generator_fallback";
        #ifdef _WIN32
            fallbackName += ".exe";
        #endif
        exePath = std::filesystem::current_path() / fallbackName;
    }
    return exePath;
}

std::filesystem::path setupOutputDirectory(const std::filesystem::path& baseDir, const std::string& dirName) {
    std::filesystem::path outputDirPath = baseDir / dirName;
    try {
        if (std::filesystem::create_directories(outputDirPath)) {
             std::cout << "Created output directory: " << outputDirPath.string() << std::endl;
        } else if (!std::filesystem::exists(outputDirPath) || !std::filesystem::is_directory(outputDirPath)) {
             std::cerr << "Error: Failed to create or access output directory: " << outputDirPath.string() << std::endl;
             return std::filesystem::path();
        }
        return outputDirPath;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error (filesystem): Creating directory " << outputDirPath.string() << ": " << e.what() << std::endl;
        return std::filesystem::path();
    }
}

} // namespace PathManager