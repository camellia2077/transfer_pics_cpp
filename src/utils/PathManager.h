#ifndef PATH_MANAGER_H
#define PATH_MANAGER_H

#include <filesystem>
#include <string>

namespace PathManager {
    std::filesystem::path getExecutablePath(int argc, char* argv[]);
    std::filesystem::path setupOutputDirectory(const std::filesystem::path& baseDir, const std::string& dirName);
}

#endif // PATH_MANAGER_H