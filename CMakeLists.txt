cmake_minimum_required(VERSION 3.12)
project(AsciiGenerator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# +++ 添加开始 +++
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
# +++ 添加结束 +++

# --------------------------------------------------------------------------
# 查找依赖库
# --------------------------------------------------------------------------

# 查找线程库 (pthreads) - 不需要手动添加 -pthread
find_package(Threads REQUIRED)

# 使用 PkgConfig 模块来查找 inih 库
find_package(PkgConfig REQUIRED)
pkg_check_modules(INIH REQUIRED IMPORTED_TARGET inih)

# --------------------------------------------------------------------------
# 添加可执行文件和源文件
# --------------------------------------------------------------------------

add_executable(ascii_generator
    main.cpp
    config_handler.cpp
    image_converter.cpp
    ascii_renderer.cpp
)

# +++ 添加开始 +++
# --------------------------------------------------------------------------
# 为目标启用 LTO (IPO) 属性 (必须在 add_executable 之后)
# --------------------------------------------------------------------------
if(ipo_supported)
    set_property(TARGET ascii_generator PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    message(STATUS "IPO / LTO enabled for target 'ascii_generator'.")
endif()
# +++ 添加结束 +++

# --------------------------------------------------------------------------
# 配置包含目录
# --------------------------------------------------------------------------

target_include_directories(ascii_generator PRIVATE
    # 注意: 如果使用 IMPORTED_TARGET PkgConfig::INIH，通常它会包含 include 目录
    # 但如果 INIH_INCLUDE_DIRS 变量确实被 pkg_check_modules 设置了，也可以用它
    # ${INIH_INCLUDE_DIRS} # 可以保留或移除，取决于 PkgConfig::INIH 的行为
    ${CMAKE_CURRENT_SOURCE_DIR} # 确保找到本地头文件
)

# --------------------------------------------------------------------------
# 链接库
# --------------------------------------------------------------------------

target_link_libraries(ascii_generator PRIVATE
    # 链接线程库 (包含 -pthread 等标志)
    Threads::Threads
    # 链接 inih 库
    PkgConfig::INIH
)

# --------------------------------------------------------------------------
# 添加编译选项 (可选)
# --------------------------------------------------------------------------
# 为 GCC/Clang (MinGW 使用 GCC) 添加优化选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # *** 修改这里：将 -O2 改为 -O3 ***
    target_compile_options(ascii_generator PRIVATE -O3 -Wall -Wextra) # 添加优化和警告
    # CMake 会在启用 IPO 时自动添加 -flto，无需手动添加
endif()

# --------------------------------------------------------------------------
# (可选) 安装规则 - 如果你想让 'make install' 工作
# --------------------------------------------------------------------------
# install(TARGETS ascii_generator DESTINATION bin) # 示例安装规则