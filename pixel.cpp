#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <stdexcept>
#include <cstdlib>
#include <sstream>
#include <filesystem>
#include <set>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


#define STB_IMAGE_RESIZE_IMPLEMENTATION 
#include "stb_image_resize2.h" 

#ifdef _WIN32
#include <windows.h> // 用于 SetConsoleOutputCP (可选，用于控制台显示)
#endif

namespace fs = std::filesystem; // 为方便起见，使用别名
using namespace std;

// --- 函数前向声明 ---
// *** 已修改: getUserInput 现在接受文件或文件夹路径 ***
bool getUserInput(fs::path& input_path, int& pixel_size);
// *** 已修改: 重命名参数以更清晰 ***
fs::path createOutputDirectory(const fs::path& parent_dir_for_output);
unsigned char* loadImage(const fs::path& filepath, int& width, int& height, int& channels);
void pixelateImage(const unsigned char* input_data, int width, int height, int channels, int pixel_size, unsigned char* output_data);
void saveImage(const fs::path& filepath, int width, int height, int channels, const unsigned char* data);
bool processImageFile(const fs::path& input_file_path, const fs::path& output_dir, int pixel_size, const string& suffix, const string& output_extension);
stbir_pixel_layout get_pixel_layout(int channels);
void* safe_malloc(size_t size);

// --- 辅助函数 (未更改) ---

// 辅助函数：安全内存分配
void* safe_malloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr && size > 0) {
        // 输出英文错误信息
        throw std::runtime_error("Memory allocation failed (malloc)");
    }
    return ptr;
}

// 辅助函数：根据通道数获取像素布局 (Pixel Layout)
stbir_pixel_layout get_pixel_layout(int channels) {
    switch (channels) {
        case 1: return STBIR_1CHANNEL;
        case 2: return STBIR_2CHANNEL;
        case 3: return STBIR_RGB;       // 假设为 RGB 顺序
        case 4: return STBIR_RGBA;      // 假设为 RGBA 顺序
        default:
            // 输出英文错误信息
            throw std::runtime_error("Unsupported number of channels: " + std::to_string(channels));
    }
    // 注意：如果输入是 BGR 等格式，或需要预乘 Alpha (STBIR_RGBA_PM)，请进行调整
}

// --- 修改和新增的函数 ---

/**
 * @brief 获取用户输入的文件夹或单个文件路径，以及块大小。
 * @param input_path 用于存储输入路径的引用。
 * @param pixel_size 用于存储输入块大小的引用。
 * @return 如果输入语法有效则返回 true，否则返回 false。
 */
bool getUserInput(fs::path& input_path, int& pixel_size) {
    string path_str;
    // *** 已修改: 更新为英文提示 ***
    cout << "Input FOLDER or IMAGE path: ";
    // 使用 getline 处理带空格的路径
    getline(cin, path_str);
    // 去除首尾引号和空格 (可选，但建议这样做)
    path_str.erase(0, path_str.find_first_not_of(" \t\n\r\f\v\""));
    path_str.erase(path_str.find_last_not_of(" \t\n\r\f\v\"") + 1);

    if (path_str.empty()) {
        // 输出英文错误信息
        cerr << "Error: Input path cannot be empty." << endl;
        return false;
    }

    try {
        input_path = fs::path(path_str);
         // 尝试规范化路径 (可选，但可以处理 ./ ../ 等)
         // input_path = fs::absolute(input_path); // 转为绝对路径
         // input_path = fs::weakly_canonical(input_path); // 尝试规范化，不存在的部分不报错
    } catch (const std::exception& e) {
        // 输出英文错误信息
        cerr << "Error: Invalid path format '" << path_str << "'. " << e.what() << endl;
        return false;
    }

    // 输出英文提示
    cout << "Input block size (e.g., 8, 16): ";
    if (!(cin >> pixel_size)) {
        // 输出英文错误信息
        cerr << "Error: Invalid input for block size. Please enter an integer." << endl;
        cin.clear(); // 清除错误标志
        cin.ignore(numeric_limits<streamsize>::max(), '\n'); // 丢弃错误输入
        return false;
    }
    // 读取整数后，消耗掉剩余的换行符
    cin.ignore(numeric_limits<streamsize>::max(), '\n');


    if (pixel_size <= 0) {
        // 输出英文错误信息
        cerr << "Error: Block size must be a positive integer." << endl;
        return false;
    }
    return true;
}

/**
 * @brief 在指定的父目录下创建一个名为 "pixelated_output" 的输出子目录。
 * @param parent_dir_for_output 将在其中创建子目录的父目录路径。
 * @return 创建的（或已存在的）输出子目录的路径。
 * @throws std::runtime_error 如果无法创建输出目录。
 */
fs::path createOutputDirectory(const fs::path& parent_dir_for_output) {
    // *** 已修改: 使用传入的父目录路径 ***
    fs::path output_subdir = parent_dir_for_output / "pixelated_output";
    try {
        if (!fs::exists(output_subdir)) {
            // *** 已修改: 输出英文消息 ***
            cout << "Creating output subdirectory in '" << parent_dir_for_output.string() << "'..." << endl;
            if (fs::create_directory(output_subdir)) {
                // 输出英文消息
                cout << " -> Created output subdirectory: " << output_subdir.string() << endl;
            } else {
                 // 输出英文错误信息
                throw std::runtime_error("Failed to create output subdirectory (unknown reason): " + output_subdir.string());
            }
        } else if (!fs::is_directory(output_subdir)) {
             // 输出英文错误信息
            throw std::runtime_error("Output path exists but is not a directory: " + output_subdir.string());
        } else {
             // 输出英文消息
             cout << "Output subdirectory already exists: " << output_subdir.string() << endl;
        }
    } catch (const fs::filesystem_error& e) {
         // 输出英文错误信息
        throw std::runtime_error("Filesystem error creating output subdirectory: " + output_subdir.string() + " - " + e.what());
    }
    return output_subdir; // 返回子目录的路径
}


/**
 * @brief 从指定的文件路径加载图像。
 * @param filepath 图像文件的路径。
 * @param width 用于存储图像宽度的引用。
 * @param height 用于存储图像高度的引用。
 * @param channels 用于存储颜色通道数的引用。
 * @return 指向加载的图像数据的指针。失败时抛出 std::runtime_error。
 * @note 调用者负责使用 stbi_image_free() 释放返回的指针。
 */
unsigned char* loadImage(const fs::path& filepath, int& width, int& height, int& channels) {
    // 使用 filepath.string() 或 filepath.u8string() 以保证兼容性
    // 对于非 ASCII 路径，尤其是在 Windows 上，u8string 通常更安全
    #ifdef _WIN32
        std::string path_str = filepath.string(); // 或者，如果需要用于旧版 API，则使用 WideCharToMultiByte
    #else
        std::string path_str = filepath.string();
    #endif

    // 输出英文消息
    cout << "Loading image '" << path_str << "'..." << endl;

    unsigned char* data = stbi_load(path_str.c_str(), &width, &height, &channels, 0);
    if (!data) {
        std::stringstream ss;
        // 输出英文错误信息
        ss << "Error: Cannot load image " << path_str << ". STB Error: " << (stbi_failure_reason() ? stbi_failure_reason() : "Unknown error");
        throw std::runtime_error(ss.str());
    }
    // 输出英文消息
    cout << " -> Loaded: " << width << "x" << height << ", " << channels << " channels" << std::endl;
    return data;
}

/**
 * @brief 使用调整大小的方法（先缩小后放大）对图像进行像素化处理。
 * @param input_data 指向原始图像数据的指针。
 * @param width 原始图像的宽度。
 * @param height 原始图像的高度。
 * @param channels 颜色通道的数量。
 * @param pixel_size 像素块的目标大小。
 * @param output_data 指向为像素化图像预分配的缓冲区的指针。
 * @throws std::runtime_error 如果调整大小失败或参数无效。
 */
void pixelateImage(const unsigned char* input_data, int width, int height, int channels, int pixel_size, unsigned char* output_data) {
     if (!input_data || !output_data || width <= 0 || height <= 0 || channels <= 0 || pixel_size <= 0) {
          // 输出英文错误信息
         throw std::runtime_error("Invalid arguments passed to pixelateImage.");
     }

     // --- 计算中间小图像的尺寸 ---
     int small_width = std::max(1, static_cast<int>(std::round(static_cast<double>(width) / pixel_size)));
     int small_height = std::max(1, static_cast<int>(std::round(static_cast<double>(height) / pixel_size)));
     // cout << "Target small dimensions: " << small_width << "x" << small_height << endl; // 调试注释保持不变

     // --- 为中间小图像分配内存 ---
     size_t small_image_size = static_cast<size_t>(small_width) * small_height * channels;
     unsigned char* small_image_data = nullptr; // 初始化为 nullptr
     try {
         small_image_data = static_cast<unsigned char*>(safe_malloc(small_image_size));

         stbir_pixel_layout layout = get_pixel_layout(channels); // 获取一次布局信息

         // --- 1. 缩小图像 (使用 POINT_SAMPLE/最近邻采样) ---
         // cout << "Calling stbir_resize (shrink)..." << endl; // 调试注释保持不变
         void* resize_result_shrink_ptr = stbir_resize(
             input_data, width, height, width * channels,           // 输入
             small_image_data, small_width, small_height, small_width * channels, // 输出
             layout,                // 像素布局
             STBIR_TYPE_UINT8,      // 数据类型
             STBIR_EDGE_CLAMP,      // 边缘模式
             STBIR_FILTER_POINT_SAMPLE   // 滤波器 (最近邻)
         );
         if (!resize_result_shrink_ptr) {
              // 输出英文错误信息
             throw std::runtime_error("Error: Failed to shrink image using stbir_resize.");
         }
         // cout << "Image shrink successful." << endl; // 调试注释保持不变

         // --- 2. 将图像放大回原始尺寸 (使用 POINT_SAMPLE/最近邻采样) ---
         // cout << "Calling stbir_resize (enlarge)..." << endl; // 调试注释保持不变
         void* resize_result_enlarge_ptr = stbir_resize(
             small_image_data, small_width, small_height, small_width * channels, // 输入
             output_data, width, height, width * channels,        // 输出 (由调用者预分配)
             layout,                // 像素布局
             STBIR_TYPE_UINT8,      // 数据类型
             STBIR_EDGE_CLAMP,      // 边缘模式
             STBIR_FILTER_POINT_SAMPLE   // 滤波器 (最近邻)
         );
         if (!resize_result_enlarge_ptr) {
             // 输出英文错误信息
             throw std::runtime_error("Error: Failed to enlarge image using stbir_resize.");
         }
         // cout << "Image enlarge successful." << endl; // 调试注释保持不变

         // --- 释放中间小图像数据 ---
         free(small_image_data); // 释放临时缓冲区
         small_image_data = nullptr; // 避免悬空指针问题

     } catch (...) {
         // 确保即使在分配后发生错误，中间缓冲区也会被释放
         free(small_image_data);
         // 重新抛出捕获到的异常
         throw;
     }
}


/**
 * @brief 将图像数据保存为 PNG 文件。
 * @param filepath 保存图像的路径（包括文件名）。
 * @param width 图像的宽度。
 * @param height 图像的高度。
 * @param channels 颜色通道的数量。
 * @param data 指向要保存的图像数据的指针。
 * @throws std::runtime_error 如果写入图像失败或数据为 null。
 */
void saveImage(const fs::path& filepath, int width, int height, int channels, const unsigned char* data) {
    if (!data) {
         // 输出英文错误信息
        throw std::runtime_error("Error: Cannot save null image data.");
    }
     // 使用 filepath.string() 或 filepath.u8string()
    #ifdef _WIN32
        std::string path_str = filepath.string();
    #else
        std::string path_str = filepath.string();
    #endif

    // 输出英文消息
    cout << " -> Saving image to: " << path_str << endl;
    int write_result = stbi_write_png(path_str.c_str(), width, height, channels, data, width * channels);
    if (!write_result) {
         // 输出英文错误信息
        throw std::runtime_error("Error: Cannot write image to " + path_str);
    }
    // 输出英文消息
    cout << " -> Pixelated image saved successfully." << std::endl;
}

/**
 * @brief 处理单个图像文件：加载、像素化并保存。
 * @param input_file_path 输入图像文件的路径。
 * @param output_dir 输出图像应保存到的目录路径。
 * @param pixel_size 期望的像素块大小。
 * @param suffix 添加到输出文件名（扩展名前）的后缀。
 * @param output_extension 期望的输出文件扩展名（例如 ".png"）。
 * @return 如果处理成功则返回 true，否则返回 false。
 */
bool processImageFile(const fs::path& input_file_path, const fs::path& output_dir, int pixel_size, const string& suffix, const string& output_extension) {
    int width = 0, height = 0, channels = 0;
    unsigned char* input_image_data = nullptr;
    unsigned char* output_image_data = nullptr;
    bool success = false;

    try {
        // --- 加载输入图像 ---
        input_image_data = loadImage(input_file_path, width, height, channels);

        // --- 生成输出文件名 ---
        fs::path stem = input_file_path.stem(); // 不带扩展名的文件名
        fs::path output_filename = stem.string() + suffix + output_extension;
        fs::path output_file_path = output_dir / output_filename;

        // --- 为输出图像分配内存 ---
        size_t output_image_size = static_cast<size_t>(width) * height * channels;
         if (width > 0 && height > 0 && output_image_size / width / height != channels) {
             throw std::overflow_error("Image dimensions result in memory size overflow.");
         }
        output_image_data = static_cast<unsigned char*>(safe_malloc(output_image_size));

        // --- 执行像素化处理 ---
        pixelateImage(input_image_data, width, height, channels, pixel_size, output_image_data);

        // --- 保存处理后的图像 ---
        saveImage(output_file_path, width, height, channels, output_image_data);

        success = true;

    } catch (const std::exception& e) {
         // 输出英文错误信息
        cerr << " !! Error processing file '" << input_file_path.string() << "': " << e.what() << endl;
        success = false;
    }

    // --- 释放此文件分配的所有内存 ---
    stbi_image_free(input_image_data); // 在 nullptr 上调用是安全的
    free(output_image_data);           // 在 nullptr 上调用是安全的

    return success;
}


// --- 主函数 (业务流程编排) ---
int main() {
#ifdef _WIN32
    // 可选：将控制台设置为 UTF-8 (注释保持不变)
    // SetConsoleOutputCP(CP_UTF8);
    // SetConsoleCP(CP_UTF8);
#endif

    fs::path input_path;
    int pixel_size = 0;
    fs::path output_dir; // 最终的输出目录 (pixelated_output)
    fs::path base_dir_for_output; // 用于确定输出子目录位置的基准目录

    // --- 1. 获取用户输入 ---
    if (!getUserInput(input_path, pixel_size)) {
        return 1; // 如果输入无效则退出
    }

    // --- 2. 验证输入路径是否存在，并确定是文件还是目录 ---
    std::error_code ec; // 用于检查文件系统错误而不抛出异常
    bool is_input_directory = false;
    bool is_input_file = false;

    if (!fs::exists(input_path, ec) || ec) {
         // 输出英文错误信息
        cerr << "Error: Input path does not exist or cannot be accessed: " << input_path.string() << (ec ? " (" + ec.message() + ")" : "") << endl;
        return 1;
    }

    if (fs::is_directory(input_path, ec) && !ec) {
        is_input_directory = true;
        base_dir_for_output = input_path; // 基准目录就是输入的目录
        // 输出英文消息
        cout << "Input path is a directory." << endl;
    } else if (fs::is_regular_file(input_path, ec) && !ec) {
        is_input_file = true;
        base_dir_for_output = input_path.parent_path(); // 基准目录是输入文件的父目录
         // 输出英文消息
        cout << "Input path is a file." << endl;
    } else {
         // 输出英文错误信息
        cerr << "Error: Input path is neither a directory nor a regular file: " << input_path.string() << (ec ? " (" + ec.message() + ")" : "") << endl;
        return 1;
    }

    // --- 3. 基于基准目录创建输出子目录 ---
    try {
        output_dir = createOutputDirectory(base_dir_for_output); // output_dir 现在是 ".../pixelated_output"
    } catch (const std::exception& e) {
         // 输出英文错误信息
        cerr << "Error creating output directory: " << e.what() << endl;
        return 1;
    }

    // --- 整个过程的计时变量 ---
    auto overall_start_time = std::chrono::high_resolution_clock::now();

    // --- 定义支持的图像扩展名 (不区分大小写) ---
    const std::set<std::string> image_extensions = {
        ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif"
        // 如果需要，可以添加更多
    };

    // --- 计数器 ---
    int files_processed = 0;
    int files_succeeded = 0;
    int files_failed = 0;
    int files_skipped = 0;

    // --- 4. 根据输入类型执行处理 ---
    if (is_input_directory) {
        // 处理目录中的所有文件
        // 输出英文消息
        cout << "\nProcessing files in directory: " << input_path.string() << endl;
        cout << "-------------------------------------------" << endl;
        try {
            for (const auto& entry : fs::directory_iterator(input_path)) {
                const auto& current_path = entry.path();
                 std::error_code entry_ec;
                if (fs::is_regular_file(entry.status(entry_ec)) && !entry_ec) { // 检查是否为常规文件
                    string ext = current_path.extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower); // 将扩展名转换为小写

                    if (image_extensions.count(ext)) { // 检查扩展名是否受支持
                        files_processed++;
                        // 输出英文消息
                        cout << "\n[" << files_processed << "] Processing: " << current_path.filename().string() << endl;
                        if (processImageFile(current_path, output_dir, pixel_size, "_pixelated_resize", ".png")) {
                            files_succeeded++;
                        } else {
                            files_failed++;
                        }
                    } else {
                         // 可选：记录跳过的文件 (英文消息)
                         // cout << "Skipping non-image file: " << current_path.filename().string() << endl;
                        files_skipped++;
                    }
                } else if(entry_ec) {
                     // 输出英文警告
                     cerr << " !! Warning: Could not get status for '" << current_path.string() << "': " << entry_ec.message() << endl;
                     files_skipped++;
                }
                // 可选：如果需要，处理子目录（例如，使用 recursive_directory_iterator）(注释保持不变)
            }
        } catch (const fs::filesystem_error& e) {
             // 输出英文错误信息
            cerr << "\n !! Filesystem error during directory iteration: " << e.what() << endl;
            // 如果可能，继续执行到摘要部分
        } catch (const std::exception& e) {
             // 输出英文错误信息
             cerr << "\n !! An unexpected error occurred during directory processing: " << e.what() << endl;
            // 如果可能，继续执行到摘要部分
        }
    }
    else if (is_input_file) {
        // 处理单个文件
        // 输出英文消息
        cout << "\nProcessing single file: " << input_path.string() << endl;
        cout << "-------------------------------------------" << endl;
        string ext = input_path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower); // 转换为小写

        if (image_extensions.count(ext)) { // 检查扩展名
            files_processed = 1; // 只处理一个文件
            if (processImageFile(input_path, output_dir, pixel_size, "_pixelated_resize", ".png")) {
                files_succeeded = 1;
            } else {
                files_failed = 1;
            }
        } else {
            // 输出英文错误信息
            cerr << "Error: Input file is not a supported image type (supported: ";
            for(const auto& supp_ext : image_extensions) cerr << supp_ext << " ";
            cerr << ")" << endl;
            files_skipped = 1; // 跳过这一个文件
            files_processed = 0; // 没有成功处理任何文件
        }
    }

    // --- 5. 输出摘要和计时 ---
    auto overall_end_time = std::chrono::high_resolution_clock::now();
    chrono::duration<double, std::milli> duration_ms = overall_end_time - overall_start_time;

    cout << "\n-------------------------------------------" << endl;
    // 输出英文摘要
    cout << "Processing Summary:" << endl;
    if (is_input_directory) {
        cout << " - Input Type: Directory" << endl;
        cout << " - Total items encountered in directory: " << (files_processed + files_skipped) << endl;
        cout << " - Image files processed:              " << files_processed << endl;
        cout << " - Files skipped (non-image/error):  " << files_skipped << endl;
    } else {
         cout << " - Input Type: Single File" << endl;
         cout << " - File processed:                   " << files_processed << endl; // Will be 0 or 1
         cout << " - File skipped (wrong type/error):  " << files_skipped << endl; // Will be 0 or 1
    }
    cout << " - Successfully pixelated:           " << files_succeeded << endl;
    cout << " - Failed to pixelate:               " << files_failed << endl;
    cout << "-------------------------------------------" << endl;
    cout << "Total Processing Time:" << endl;
    cout << std::fixed << std::setprecision(3)
         << duration_ms.count() / 1000.0 << " seconds ("
         << duration_ms.count() << " ms)" << std::endl;
    cout << "-------------------------------------------" << endl;
    // 输出英文消息
    cout << "Output saved to: " << output_dir.string() << endl;
    cout << "-------------------------------------------" << endl;

    // 如果所有尝试处理的图像都成功，则返回 0，否则返回 1
    // 对于单个文件，如果处理失败或被跳过，也返回 1
    return (files_failed == 0 && files_succeeded > 0) ? 0 : 1;
}
