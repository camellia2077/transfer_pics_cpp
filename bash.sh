#!/bin/bash

# MSYS2/MinGW 自动编译脚本
#
# 使用方法:
# 1. 将此脚本保存为 build.sh 或 bash.sh 并放置在您项目根目录。
# 2. 在 MSYS2 MinGW 64-bit 终端中，给脚本添加执行权限: chmod +x bash.sh
# 3. 运行脚本: ./bash.sh

# --- 配置 ---
BUILD_DIR="build"

# --- 脚本主体 ---
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}Starting the automated build process...${NC}"

# 1. 检查并删除旧的构建目录
if [ -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}Build directory '$BUILD_DIR' already exists. Removing it for a clean build.${NC}"
    rm -rf "$BUILD_DIR"
    if [ $? -ne 0 ]; then
        echo -e "${RED}Error: Failed to remove directory '$BUILD_DIR'. Please check permissions.${NC}"
        exit 1
    fi
fi

# 2. 创建新的构建目录并进入
echo -e "${GREEN}Creating a new build directory: '$BUILD_DIR'${NC}"
mkdir "$BUILD_DIR"
if [ $? -ne 0 ]; then
    echo -e "${RED}Error: Failed to create directory '$BUILD_DIR'.${NC}"
    exit 1
fi

pushd "$BUILD_DIR" > /dev/null

# 3. 运行 CMake 来配置项目
echo -e "${GREEN}Running CMake to configure the project...${NC}"
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..

if [ $? -ne 0 ]; then
    echo -e "${RED}CMake configuration failed. Aborting build.${NC}"
    popd > /dev/null
    exit 1
fi

# 4. 运行 mingw32-make 来编译项目
#    *** 修改点：明确使用 mingw32-make 以避免路径冲突 ***
echo -e "${GREEN}Running make to compile the project...${NC}"
mingw32-make

if [ $? -ne 0 ]; then
    echo -e "${RED}Make process failed. Check the compilation errors above.${NC}"
    popd > /dev/null
    exit 1
fi

# 返回原始目录
popd > /dev/null

echo -e "${GREEN}Build process completed successfully!${NC}"
echo -e "The executable is located inside the '${YELLOW}$BUILD_DIR${NC}' directory."

