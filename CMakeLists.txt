cmake_minimum_required(VERSION 3.12)
project(AsciiGenerator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)


# --------------------------------------------------------------------------
# 检查并准备启用链接时优化 (LTO / IPO)
# --------------------------------------------------------------------------
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT output)
if(ipo_supported)
    message(STATUS "IPO / LTO is supported by toolchain, will be enabled for target.")
else()
    message(WARNING "IPO / LTO not supported by toolchain: ${output}")
endif()

# --------------------------------------------------------------------------
# 查找依赖库
# --------------------------------------------------------------------------

# 查找线程库 (pthreads)
find_package(Threads REQUIRED)

# 查找 nlohmann_json 库 (for Modern C++)
# MSYS2/MinGW 通常通过 `pacman -S mingw-w64-x86_64-nlohmann-json` 安装
# 这会提供一个可以让 find_package 工作的配置文件
find_package(nlohmann_json 3.2.0 REQUIRED)
message(STATUS "Found nlohmann_json library.")


# --------------------------------------------------------------------------
# 添加可执行文件和源文件
# --------------------------------------------------------------------------

add_executable(ascii_generator
    main.cpp
    config_handler.cpp
    image_converter.cpp
    ascii_renderer.cpp
)

# --------------------------------------------------------------------------
# 为目标启用 LTO (IPO) 属性 (必须在 add_executable 之后)
# --------------------------------------------------------------------------
if(ipo_supported)
    set_property(TARGET ascii_generator PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    message(STATUS "IPO / LTO enabled for target 'ascii_generator'.")
endif()

# --------------------------------------------------------------------------
# 配置包含目录
# --------------------------------------------------------------------------

target_include_directories(ascii_generator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} # 确保找到本地头文件
)

# --------------------------------------------------------------------------
# 链接库
# --------------------------------------------------------------------------

target_link_libraries(ascii_generator PRIVATE
    # 链接线程库
    Threads::Threads
    # 链接 nlohmann_json
    nlohmann_json::nlohmann_json
)

# --------------------------------------------------------------------------
# 添加编译选项 (可选)
# --------------------------------------------------------------------------
# 为 GCC/Clang (MinGW 使用 GCC) 添加优化选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ascii_generator PRIVATE -O3 -Wall -march=native)
endif()

# --------------------------------------------------------------------------
# 添加链接器选项以剥离调试信息
# --------------------------------------------------------------------------
if(CMAKE_BUILD_TYPE MATCHES "Release")
    target_link_options(ascii_generator PRIVATE -s)
    message(STATUS "Stripping debug symbols for target 'ascii_generator' in Release build.")
endif()
