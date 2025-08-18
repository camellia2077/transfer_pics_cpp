# CMake 最低版本要求
cmake_minimum_required(VERSION 3.16)

# 项目名称
project(AsciiArtGenerator LANGUAGES CXX)

# --- 新增：启用 ccache  ---
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    message(STATUS "Found ccache: ${CCACHE_FOUND}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_FOUND}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CCACHE_FOUND}")
else()
    message(STATUS "ccache not found. Proceeding without it.")
endif()

# 设置 C++ 标准为 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 编译选项优化 ---
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os -s")

# --- 查找依赖库 ---
find_package(nlohmann_json 3.2.0 REQUIRED)
message(STATUS "Found nlohmann_json: ${nlohmann_json_VERSION}")
find_package(Threads REQUIRED)
find_path(STB_INCLUDE_DIR NAMES stb/stb_image.h HINTS /ucrt64/include)

if(STB_INCLUDE_DIR)
    message(STATUS "Found stb headers in: ${STB_INCLUDE_DIR}")
    add_library(STB INTERFACE)
    target_include_directories(STB INTERFACE ${STB_INCLUDE_DIR})
else()
    message(FATAL_ERROR "Could not find stb headers.")
endif()


# --- 定义可执行文件和源文件 ---
set(CONFIG_SOURCES src/config/config_handler.cpp)
set(CONVERSION_SOURCES src/conversion/image_converter.cpp)
set(RENDERING_SOURCES
    src/rendering/PngRenderer.cpp
    src/rendering/HtmlRenderer.cpp
)
set(APP_SOURCES src/app/application.cpp)
set(CORE_SOURCES src/core/processing_orchestrator.cpp)
set(UI_SOURCES src/ui/cli_handler.cpp)
set(UTILS_SOURCES src/utils/PathManager.cpp)

# 组合所有源文件
set(SOURCES
    src/main.cpp
    src/common/pch.cpp # <-- 新增：添加 pch.cpp 到源文件列表
    ${CONFIG_SOURCES}
    ${CONVERSION_SOURCES}
    ${RENDERING_SOURCES}
    ${APP_SOURCES}
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${UTILS_SOURCES}
)
# 添加可执行文件目标
add_executable(ascii_generator ${SOURCES})


# --- 为目标添加头文件包含目录 ---
target_include_directories(ascii_generator
    PRIVATE
    src
    src/app
    src/common
    src/config
    src/conversion
    src/core
    src/rendering
    src/ui
    src/utils
)

# --- 新增：配置预编译头文件 ---
target_precompile_headers(ascii_generator
    PRIVATE
    # CMake 会自动将这个头文件包含在所有源文件的编译中
    src/common/pch.h
)


# --- 链接依赖库到目标文件 ---
target_link_libraries(ascii_generator
    PRIVATE
    nlohmann_json::nlohmann_json
    Threads::Threads
    STB
)

# --- 平台特定设置 ---
if(WIN32)
    target_link_libraries(ascii_generator PRIVATE user32)
endif()

# --- 自定义命令：在构建后复制资源文件 ---
add_custom_command(TARGET ascii_generator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/config/config.json"
    "$<TARGET_FILE_DIR:ascii_generator>"
    COMMENT "Copying config.json to build directory"
)
add_custom_command(TARGET ascii_generator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/fonts/SourceCodePro-Regular.ttf"
    "$<TARGET_FILE_DIR:ascii_generator>"
    COMMENT "Copying font file to build directory"
)


# --- 安装规则 (可选) ---
install(TARGETS ascii_generator DESTINATION bin)

# --- 输出信息 ---
message(STATUS "Configuration finished. You can now build the project.")