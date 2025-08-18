#!/bin/bash

# build.sh - 自动编译脚本

# 如果出错则立即退出
set -e

# 核心修改：切换到脚本所在的目录
# 这使得脚本可以从任何位置运行，并且总是在正确的项目根目录中工作。
SCRIPT_DIR="$(dirname "$0")"
echo "Changing directory to: $SCRIPT_DIR"
cd "$SCRIPT_DIR"

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
# 核心修改：在 CMake 命令中显式设置构建类型为 Release
cmake -DCMAKE_BUILD_TYPE=Release ..

# 运行 CMake 的通用构建命令
# 这会自动调用 Ninja (或 Make，取决于 CMake 的配置)
echo "Building project..."
cmake --build .

# 提示用户编译完成
echo ""
echo "Build complete!"
echo "The executable is located at: $(pwd)/ascii_generator.exe"
echo "You can run it from the project root directory using: ./build/ascii_generator.exe"