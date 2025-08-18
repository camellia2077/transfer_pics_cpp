#ifndef APPLICATION_H
#define APPLICATION_H

#include "common_types.h"

class Application {
public:
    Application(int argc, char* argv[]);
    int run();

private:
    bool initialize();
    bool resolveFontPath();

    int m_argc;
    char** m_argv;
    Config m_config;
    std::filesystem::path m_exeDir;
};

#endif // APPLICATION_H