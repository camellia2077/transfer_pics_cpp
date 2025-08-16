#!/bin/bash

# build.sh - 自动编译脚本
#
# 1. 创建一个 build 目录 (如果不存在)
# 2. 进入 build 目录
# 3. 运行 CMake 来配置项目
# 4. 运行 CMake 的构建命令来编译项目

# 如果出错则立即退出
set -e

# 创建构建目录
BUILD_DIR="build"
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory: $BUILD_DIR"
    mkdir "$BUILD_DIR"
fi

# 进入构建目录
cd "$BUILD_DIR"

# 运行 CMake 配置
# ".." 指向上级目录，也就是 CMakeLists.txt 所在的位置
echo "Configuring project with CMake..."
cmake ..

# 运行 CMake 的通用构建命令
# 这会自动调用 Ninja (或 Make，取决于 CMake 的配置)
echo "Building project..."
cmake --build .

# 提示用户编译完成
echo ""
echo "Build complete!"
echo "The executable is located at: $(pwd)/ascii_generator.exe"
echo "You can run it from the project root directory using: ./build/ascii_generator.exe"