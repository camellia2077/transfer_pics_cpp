// src/main.cpp

#include "app/application.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) {
    try {
        Application app(argc, argv);
        return app.run();
    } catch (const std::exception& e) {
        std::cerr << "An unrecoverable, unexpected error occurred: " << e.what() << std::endl;
        return 1; // 发生严重错误时，直接返回错误码退出
    } catch (...) {
        std::cerr << "An unknown, unrecoverable error occurred." << std::endl;
        return 1; // 发生未知严重错误时，直接返回错误码退出
    }
}