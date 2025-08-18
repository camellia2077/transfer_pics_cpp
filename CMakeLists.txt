# CMake 最低版本要求
cmake_minimum_required(VERSION 3.16)

# 项目名称
project(AsciiArtGenerator LANGUAGES CXX)

# 设置 C++ 标准为 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 编译选项优化 ---
# 开启 Release 模式下的编译优化
# CMAKE_CXX_FLAGS_RELEASE 仅在构建类型为 Release 时生效
# -Os: 优化代码大小
# -s: 剥离可执行文件中的调试符号
# -flto: 开启链接时优化，可以进一步优化代码
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os -s")

# --- 查找依赖库 ---

# 1. 查找 nlohmann_json 库
find_package(nlohmann_json 3.2.0 REQUIRED)
message(STATUS "Found nlohmann_json: ${nlohmann_json_VERSION}")

# 2. 查找线程库
find_package(Threads REQUIRED)

# 3. 查找 stb 库 (作为纯头文件库)
# 我们直接查找一个标志性的头文件来定位 include 目录
find_path(STB_INCLUDE_DIR
    NAMES stb/stb_image.h  # 查找 'stb/stb_image.h' 这个文件
    # MSYS2 UCRT64 环境下的默认头文件搜索路径
    HINTS /ucrt64/include
    DOC "Path to the stb header directory"
)

if(STB_INCLUDE_DIR)
    message(STATUS "Found stb headers in: ${STB_INCLUDE_DIR}")
    # 创建一个名为 "STB" 的 INTERFACE 库。这是一个由我们项目定义的目标。
    add_library(STB INTERFACE)
    # 为我们自己创建的 "STB" 目标添加 include 目录。
    # INTERFACE 关键字意味着任何链接到 STB 的目标都会自动继承这个 include 目录。
    target_include_directories(STB INTERFACE ${STB_INCLUDE_DIR})
else()
    message(FATAL_ERROR "Could not find stb headers. Please ensure it's installed via 'pacman -S mingw-w64-ucrt-x86_64-stb'")
endif()


# --- 定义可执行文件 ---

# 包含 src 目录
include_directories(src)

# 搜集所有 src 目录下的 .cpp 源文件
file(GLOB_RECURSE SOURCES "src/*.cpp")

# 添加可执行文件目标
add_executable(ascii_generator ${SOURCES})


# --- 链接依赖库到目标文件 ---

target_link_libraries(ascii_generator
    PRIVATE
    nlohmann_json::nlohmann_json
    Threads::Threads
    STB # 使用新的、正确的 INTERFACE 目标
)

# --- 平台特定设置 ---
if(WIN32)
    target_link_libraries(ascii_generator PRIVATE user32)
endif()

# --- 安装规则 (可选) ---
install(TARGETS ascii_generator DESTINATION bin)

# --- 输出信息 ---
message(STATUS "Configuration finished. You can now build the project.")