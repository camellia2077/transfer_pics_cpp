#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream>      // 用于读取字体文件和配置文件
#include <memory>       // 用于 unique_ptr
#include <filesystem>   // 用于路径操作和目录创建 (需要 C++17)
#include <system_error> // 用于文件系统错误处理
#include <chrono>       // 用于高精度计时
#include <iomanip>      // 用于格式化输出 (setprecision)
#include <sstream>      // 用于字符串流转换
#include <cctype>       // 用于 tolower
#include <set>          // 用于存储支持的扩展名

// 定义此宏，以便 stb_image.h 包含实现
// 确保此定义仅出现在一个 .cpp 文件中
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // 假设 stb_image.h 在同一目录或包含路径中

// 定义 stb_image_write 的实现
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h" // 假设 stb_image_write.h 在这里

// 定义 stb_truetype 的实现
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h" // 假设 stb_truetype.h 在这里

// --- 将 std 命名空间引入作用域 ---
using namespace std;
// --- 将特定的嵌套命名空间成员引入作用域 ---
using std::filesystem::path; // 现在可以直接使用 'path'
using namespace std::chrono; // 将 chrono 成员引入作用域

// --- 全局常量和类型 ---
const string ASCII_CHARS = "@%#*+=-:. ";
const int NUM_ASCII_CHARS = static_cast<int>(ASCII_CHARS.length());
const int OUTPUT_CHANNELS = 3; // 输出 PNG 为 RGB 格式

// 支持的图片文件扩展名 (小写)
const set<string> SUPPORTED_EXTENSIONS = {
    ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif" // 注意: GIF 可能只加载第一帧
};

enum class ColorScheme {
    BLACK_ON_WHITE, WHITE_ON_BLACK, GREEN_ON_BLACK, PURPLE_ON_BLACK,
    COLOR_ON_WHITE, COLOR_ON_BLACK
};

struct CharColorInfo {
    char character;
    unsigned char color[3]; // 来自原始图像的 R, G, B 值
};

// --- 新增：配置结构体 ---
struct Config {
    int targetWidth = 1024;
    double charAspectRatioCorrection = 2.0;
    string fontFilename = "Consolas.ttf";
    float fontSize = 15.0f;
    string finalFontPath = ""; // 将由 main 计算

    // --- 其他处理相关的常量 ---
    string baseOutputFilename = "output_ascii_art";
    string outputExtension = ".png";
    // 对于单个文件输入，这是相对于文件父目录的子目录名
    // 对于文件夹输入，这是在主输出目录下为每个图片创建的子目录名的后缀
    string imageOutputSubDirSuffix = "_ascii_output";
    // 对于文件夹输入，这是在输入文件夹旁边创建的主输出目录的后缀
    string batchOutputSubDirSuffix = "_ascii_batch_output";

    vector<ColorScheme> schemesToGenerate = { ColorScheme::COLOR_ON_WHITE };
};


// --- 辅助函数 ---

// 去除字符串首尾空格的辅助函数
string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\n\r\f\v");
    if (string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(first, (last - first + 1));
}

// 检查文件扩展名是否为支持的图像格式 (不区分大小写)
bool isImageFile(const path& p) {
    if (!p.has_extension()) {
        return false;
    }
    string ext = p.extension().string();
    transform(ext.begin(), ext.end(), ext.begin(), ::tolower); // 转为小写
    return SUPPORTED_EXTENSIONS.count(ext);
}

// 将文件读入字节向量
vector<unsigned char> read_file(const string& filename) {
    ifstream file(filename, ios::binary | ios::ate);
    if (!file) {
        // 输出英文错误
        cerr << "Error: Cannot open file '" << filename << "'" << endl;
        return {};
    }
    streamsize size = file.tellg();
    file.seekg(0, ios::beg);
    vector<unsigned char> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        // 输出英文错误
        cerr << "Error: Cannot read file '" << filename << "'" << endl;
        return {};
    }
    return buffer;
}

// 根据颜色方案获取用于文件名的描述性后缀
string getSchemeSuffix(ColorScheme scheme) {
    switch (scheme) {
        case ColorScheme::BLACK_ON_WHITE:   return "_BlackOnWhite";
        case ColorScheme::WHITE_ON_BLACK:   return "_WhiteOnBlack";
        case ColorScheme::GREEN_ON_BLACK:   return "_GreenOnBlack";
        case ColorScheme::PURPLE_ON_BLACK:  return "_PurpleOnBlack";
        case ColorScheme::COLOR_ON_WHITE:   return "_ColorOnWhite";
        case ColorScheme::COLOR_ON_BLACK:   return "_ColorOnBlack";
        default:                            return "_UnknownScheme";
    }
}


// --- 配置加载函数 (修改为更新 Config 对象) ---
bool loadConfiguration(const path& configPath, Config& config) // 接受 Config 引用
{
    ifstream configFile(configPath);
    if (!configFile) {
        cout << "Info: Configuration file '" << configPath.string() << "' not found. Using default settings." << endl;
        return true; // 文件不存在是正常的，使用默认值
    }

    cout << "Info: Loading configuration from '" << configPath.string() << "'..." << endl;
    string line;
    int lineNumber = 0;
    bool loadedSuccessfully = true; // 跟踪是否有解析错误

    while (getline(configFile, line)) {
        lineNumber++;
        line = trim(line);

        // 跳过空行或注释行
        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t separatorPos = line.find('=');
        if (separatorPos == string::npos) {
            cerr << "Warning: Invalid format in config file at line " << lineNumber << ": Missing '=' separator. Line: \"" << line << "\"" << endl;
            loadedSuccessfully = false;
            continue;
        }

        string key = trim(line.substr(0, separatorPos));
        string value = trim(line.substr(separatorPos + 1));

        // 可选：将键转换为小写以便不区分大小写比较
        // transform(key.begin(), key.end(), key.begin(), ::tolower);

        if (key == "targetWidth") {
            try {
                config.targetWidth = stoi(value); // 更新 config 对象
                 cout << "  -> Loaded targetWidth = " << config.targetWidth << endl;
            } catch (const std::invalid_argument& e) {
                cerr << "Warning: Invalid value for 'targetWidth' in config file at line " << lineNumber << ". Using default. Value: \"" << value << "\"" << endl;
                loadedSuccessfully = false;
            } catch (const std::out_of_range& e) {
                 cerr << "Warning: Value out of range for 'targetWidth' in config file at line " << lineNumber << ". Using default. Value: \"" << value << "\"" << endl;
                loadedSuccessfully = false;
            }
        } else if (key == "charAspectRatioCorrection") {
             try {
                config.charAspectRatioCorrection = stod(value); // 更新 config 对象
                 cout << "  -> Loaded charAspectRatioCorrection = " << config.charAspectRatioCorrection << endl;
            } catch (const std::invalid_argument& e) {
                cerr << "Warning: Invalid value for 'charAspectRatioCorrection' in config file at line " << lineNumber << ". Using default. Value: \"" << value << "\"" << endl;
                 loadedSuccessfully = false;
            } catch (const std::out_of_range& e) {
                 cerr << "Warning: Value out of range for 'charAspectRatioCorrection' in config file at line " << lineNumber << ". Using default. Value: \"" << value << "\"" << endl;
                 loadedSuccessfully = false;
            }
        } else if (key == "fontFilename") {
            if (!value.empty()) {
                config.fontFilename = value; // 更新 config 对象
                cout << "  -> Loaded fontFilename = " << config.fontFilename << endl;
            } else {
                 cerr << "Warning: Empty value for 'fontFilename' in config file at line " << lineNumber << ". Using default." << endl;
                 loadedSuccessfully = false;
            }
        } else if (key == "fontSize") {
             try {
                config.fontSize = stof(value); // 更新 config 对象
                 cout << "  -> Loaded fontSize = " << config.fontSize << endl;
            } catch (const std::invalid_argument& e) {
                cerr << "Warning: Invalid value for 'fontSize' in config file at line " << lineNumber << ". Using default. Value: \"" << value << "\"" << endl;
                 loadedSuccessfully = false;
            } catch (const std::out_of_range& e) {
                 cerr << "Warning: Value out of range for 'fontSize' in config file at line " << lineNumber << ". Using default. Value: \"" << value << "\"" << endl;
                 loadedSuccessfully = false;
            }
        } else {
            // 可以选择性地警告未知键
             cout << "Info: Unknown key '" << key << "' in config file at line " << lineNumber << ". Ignoring." << endl;
        }
    }

    configFile.close();
    return loadedSuccessfully; // 返回true表示文件已处理，即使有警告
}


// --- 核心功能模块 ---

// 1. 获取用户输入 (不变)
string getUserInputPath() {
    string inputPathStr;
    // 输出英文提示
    cout << "Please enter the image file or folder path: ";
    getline(cin, inputPathStr);
    // 基本检查路径是否为空
    if (inputPathStr.empty()) {
        // 输出英文错误
        cerr << "Error: Input path is empty." << endl;
    }
    return inputPathStr;
}

// 2. 设置单个图片的输出子目录 (在 baseOutputDir 内创建)
path setupImageOutputSubdirectory(const path& baseOutputDir, const path& imagePath, const string& suffix) {
    string subDirName = imagePath.stem().string() + suffix; // 使用图片名（无扩展名）+ 后缀
    path outputSubDirPath = baseOutputDir / subDirName;
    try {
        // create_directories 可以创建多级尚不存在的目录
        if (filesystem::create_directories(outputSubDirPath)) {
            // 输出英文消息
            cout << "Created output subdirectory: " << outputSubDirPath.string() << endl;
        } else {
             if (!filesystem::exists(outputSubDirPath)) {
                 // 输出英文错误
                 cerr << "Error: Failed to create output subdirectory: " << outputSubDirPath.string() << endl;
                 return path(); // 失败时返回空路径
             } else if (!filesystem::is_directory(outputSubDirPath)) {
                 // 输出英文错误
                 cerr << "Error: Output path exists but is not a directory: " << outputSubDirPath.string() << endl;
                 return path(); // 失败时返回空路径
             }
             // 如果它存在并且是一个目录，我们可以继续。
             // cout << "Output subdirectory already exists: " << outputSubDirPath.string() << endl;
        }
        return outputSubDirPath; // 返回子目录的路径
    } catch (const filesystem::filesystem_error& e) {
        // 输出英文错误
        cerr << "Error: Filesystem error occurred while creating/accessing output subdirectory: " << outputSubDirPath.string() << endl;
        cerr << "Details: " << e.what() << endl;
         if (!filesystem::is_directory(outputSubDirPath)) return path(); // 如果确认失败，则返回空路径
         // 输出英文警告
         cerr << "Warning: Attempting to continue, assuming directory exists..." << endl;
         return outputSubDirPath; // 即使出现警告但目录存在，也返回路径
    } catch (const std::exception& e) {
        // 输出英文错误
        cerr << "Error: An unknown exception occurred while creating directory: " << e.what() << endl;
        return path(); // 返回空路径
    }
}


// 3. 加载输入图像 (不变)
unique_ptr<unsigned char, void(*)(void*)> loadImage(const string& imagePath, int& width, int& height) {
    unsigned char *data = stbi_load(imagePath.c_str(), &width, &height, nullptr, OUTPUT_CHANNELS); // 请求所需的通道数
    if (data == nullptr) {
        // 输出英文错误
        cerr << "Error: Failed to load image '" << imagePath << "' (or convert to RGB)" << endl;
        cerr << "STB Image Error Reason: " << stbi_failure_reason() << endl;
        return unique_ptr<unsigned char, void(*)(void*)>(nullptr, stbi_image_free); // 返回空的 unique_ptr
    }
    // 输出英文消息
    // cout << "Image loaded successfully (forced RGB): " << width << "x" << height << ", Channels: " << OUTPUT_CHANNELS << endl; // 在 processImageFile 中输出更合适
    return unique_ptr<unsigned char, void(*)(void*)>(data, stbi_image_free);
}

// 4. 生成 ASCII 数据 (不变)
vector<vector<CharColorInfo>> generateAsciiData(const unsigned char* imgData, int width, int height, int targetWidth, int targetHeight)
{
    // 输出英文消息
    // cout << "\nGenerating ASCII data..." << endl; // 在 processImageFile 中输出更合适
    vector<vector<CharColorInfo>> asciiResultData;
    if (!imgData || width <= 0 || height <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        // 输出英文错误
        cerr << "Error: Invalid arguments passed to generateAsciiData." << endl;
        return asciiResultData; // 返回空向量
    }

    asciiResultData.reserve(targetHeight);
    double xScale = static_cast<double>(width) / targetWidth;
    double yScale = static_cast<double>(height) / targetHeight;

    for (int yOut = 0; yOut < targetHeight; ++yOut) {
        vector<CharColorInfo> currentLineData;
        currentLineData.reserve(targetWidth);
        for (int xOut = 0; xOut < targetWidth; ++xOut) {
            int xImg = static_cast<int>(floor((xOut + 0.5) * xScale));
            int yImg = static_cast<int>(floor((yOut + 0.5) * yScale));
            xImg = max(0, min(xImg, width - 1));
            yImg = max(0, min(yImg, height - 1));

            size_t pixelOffset = (static_cast<size_t>(yImg) * width + xImg) * OUTPUT_CHANNELS;
            unsigned char r = imgData[pixelOffset];
            unsigned char g = imgData[pixelOffset + 1];
            unsigned char b = imgData[pixelOffset + 2];

            int gray = (static_cast<int>(r) + g + b) / 3;
            // 修正映射：在除法部分使用 NUM_ASCII_CHARS，而不是 NUM_ASCII_CHARS - 1
            int asciiIndex = static_cast<int>(floor((gray / 256.0f) * NUM_ASCII_CHARS));
            asciiIndex = max(0, min(asciiIndex, NUM_ASCII_CHARS - 1)); // 限制索引范围

            CharColorInfo info;
            info.character = ASCII_CHARS[static_cast<size_t>(asciiIndex)];
            info.color[0] = r; info.color[1] = g; info.color[2] = b;
            currentLineData.push_back(info);
        }
        asciiResultData.push_back(currentLineData);
    }
    // 将打印语句移至函数完成后 (注释保持不变)
    // cout << "ASCII data generation complete." << endl;
    return asciiResultData;
}


// 5. 加载字体信息 (不变)
bool loadFontInfo(const string& fontPath, stbtt_fontinfo& fontInfo, vector<unsigned char>& fontBuffer) {
    // 输出英文消息
    cout << "\nLoading font file: " << fontPath << " ..." << endl;
    fontBuffer = read_file(fontPath);
    if (fontBuffer.empty()) return false; // read_file 会打印错误信息

    if (!stbtt_InitFont(&fontInfo, fontBuffer.data(), stbtt_GetFontOffsetForIndex(fontBuffer.data(), 0))) {
        // 输出英文错误
        cerr << "Error: Failed to initialize font." << endl;
        return false;
    }

    // --- 尝试获取字体名称 ---
    // 输出英文默认名称
    string fontName = "Name not found";
    int length = 0;
    // 尝试使用 Microsoft Unicode BMP 编码获取“完整字体名称”（名称 ID 4）
    const char* namePtr = stbtt_GetFontNameString(&fontInfo, &length,
                                                  STBTT_PLATFORM_ID_MICROSOFT,
                                                  STBTT_MS_EID_UNICODE_BMP,
                                                  STBTT_MS_LANG_ENGLISH, // 尝试英语语言 ID
                                                  4); // 名称 ID 4：完整字体名称

    if (namePtr && length > 0) {
        // 这可能是 UTF-16BE，常用于 MS Unicode BMP。
        // 我们将尝试进行简单的转换，假设它主要与 ASCII 兼容
        // 或适用于许多终端直接使用的基本多字节字符。
        // 一个健壮的解决方案将涉及正确的 UTF-16 到 UTF-8 转换。
        string tempName;
        tempName.reserve(length / 2 + 1); // 粗略估计
        for (int i = 0; i < length; i += 2) { // 步长为 2 字节
            if (namePtr[i] == 0 && namePtr[i+1] != 0) { // 对 UTF-16BE 中的 ASCII 范围进行基本检查
                tempName += namePtr[i+1];
            } else {
                // 处理其他字符或指示非 ASCII？
                // 为简单起见，我们只对非基本字符使用 '?'
                // 或者跳过它们。这里我们添加 '?'。需要进行适当的转换才能完全支持。
                 if (namePtr[i] != 0 || namePtr[i+1] != 0) // 避免添加空字符
                     tempName += '?';
            }
        }
         if (!tempName.empty()) {
              fontName = tempName;
         } else {
              // 如果简单转换没有产生任何结果，则回退，也许是 Mac Roman？
              namePtr = stbtt_GetFontNameString(&fontInfo, &length, STBTT_PLATFORM_ID_MAC, STBTT_MAC_EID_ROMAN, STBTT_MAC_LANG_ENGLISH, 1); // 名称 ID 1：字体族名称
              if (namePtr && length > 0) {
                  fontName = string(namePtr, length);
              }
         }

    } else {
         // 回退：尝试 Mac 字体族名称（名称 ID 1）
         namePtr = stbtt_GetFontNameString(&fontInfo, &length, STBTT_PLATFORM_ID_MAC, STBTT_MAC_EID_ROMAN, STBTT_MAC_LANG_ENGLISH, 1); // 名称 ID 1：字体族名称
         if (namePtr && length > 0) {
              fontName = string(namePtr, length);
         }
         // 如果需要，可以添加更多回退（其他 ID、平台、编码）
    }
    // --- 字体名称检索尝试结束 ---

    // 输出英文消息
    cout << "Font loaded successfully." << endl;
    cout << "  -> Font Location: " << fontPath << endl;
    cout << "  -> Font Name: " << fontName << endl; // 打印检索到的名称
    return true;
}


// 6. 计算输出尺寸和字体度量 (不变)
void calculateOutputDimensions(const stbtt_fontinfo& fontInfo, float fontSize, int targetWidth, int targetHeight,
                               int& charWidth, int& lineHeight, int& outputImageWidth, int& outputImageHeight,
                               float& scale, int& ascent, int& descent, int& lineGap)
{
    scale = stbtt_ScaleForPixelHeight(&fontInfo, fontSize);
    stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
    ascent = static_cast<int>(round(ascent * scale));
    descent = static_cast<int>(round(descent * scale));
    lineGap = static_cast<int>(round(lineGap * scale));
    lineHeight = ascent - descent + lineGap;

    // 使用 'M' 估算字符宽度
    int advanceWidth, leftSideBearing;
    stbtt_GetCodepointHMetrics(&fontInfo, 'M', &advanceWidth, &leftSideBearing);
    charWidth = static_cast<int>(round(advanceWidth * scale));
    // 确保最小字符宽度，以避免稍后出现除零或零大小图像
    if (charWidth <= 0) {
        // 输出英文警告
        cerr << "Warning: Calculated character width is zero or negative. Using default value 1." << endl;
        charWidth = 1;
    }


    outputImageWidth = targetWidth * charWidth;
    outputImageHeight = targetHeight * lineHeight;

    // 确保最小尺寸
    outputImageWidth = max(1, outputImageWidth);
    outputImageHeight = max(1, outputImageHeight);

    // 输出英文消息
    // cout << "Output image dimensions calculated: " << outputImageWidth << "x" << outputImageHeight << endl; // 移至 processImageFile
}


// 7. 设置配色方案颜色 (不变)
void setSchemeColors(ColorScheme scheme, unsigned char bgColor[3], unsigned char fgColor[3]) {
    switch (scheme) {
        case ColorScheme::WHITE_ON_BLACK:
            bgColor[0] = 0;   bgColor[1] = 0;   bgColor[2] = 0;
            fgColor[0] = 255; fgColor[1] = 255; fgColor[2] = 255;
            break;
        case ColorScheme::GREEN_ON_BLACK:
            bgColor[0] = 0;   bgColor[1] = 0;   bgColor[2] = 0;
            fgColor[0] = 0;   fgColor[1] = 255; fgColor[2] = 0;
            break;
        case ColorScheme::PURPLE_ON_BLACK:
            bgColor[0] = 0;   bgColor[1] = 0;   bgColor[2] = 0;
            fgColor[0] = 128; fgColor[1] = 0;   fgColor[2] = 128;
            break;
        case ColorScheme::COLOR_ON_WHITE:
            bgColor[0] = 255; bgColor[1] = 255; bgColor[2] = 255;
            // fgColor 对于彩色方案未使用，但设置了默认值
            fgColor[0] = 0;   fgColor[1] = 0;   fgColor[2] = 0;
            break;
        case ColorScheme::COLOR_ON_BLACK:
            bgColor[0] = 0;   bgColor[1] = 0;   bgColor[2] = 0;
            // fgColor 对于彩色方案未使用，但设置了默认值
            fgColor[0] = 255; fgColor[1] = 255; fgColor[2] = 255;
            break;
        case ColorScheme::BLACK_ON_WHITE:
        default:
            bgColor[0] = 255; bgColor[1] = 255; bgColor[2] = 255;
            fgColor[0] = 0;   fgColor[1] = 0;   fgColor[2] = 0;
            break;
    }
}

// 8. 将 ASCII 艺术渲染到图像缓冲区 (不变)
void renderAsciiArt(vector<unsigned char>& outputImageData,
                    const vector<vector<CharColorInfo>>& asciiData,
                    const stbtt_fontinfo& fontInfo,
                    ColorScheme scheme,
                    const unsigned char bgColor[3], // 此方案的背景色
                    const unsigned char fgColor[3], // 固定前景色（如果使用）
                    int imgWidth, int imgHeight,
                    int ascent, float scale, int lineHeight)
{
    // 首先填充背景
    for (size_t i = 0; i < outputImageData.size(); i += OUTPUT_CHANNELS) {
        outputImageData[i]     = bgColor[0];
        outputImageData[i + 1] = bgColor[1];
        outputImageData[i + 2] = bgColor[2];
    }

    // 渲染字符
    int currentY = ascent; // 从第一个基线开始渲染
    for (const auto& lineData : asciiData) {
        int currentX = 0;
        for (const auto& charInfo : lineData) {
            char c = charInfo.character;
            int char_w, char_h, xoff, yoff;
            unsigned char* bitmap = stbtt_GetCodepointBitmap(&fontInfo, scale, scale, c, &char_w, &char_h, &xoff, &yoff);

            int advanceWidth, leftSideBearing;
            stbtt_GetCodepointHMetrics(&fontInfo, c, &advanceWidth, &leftSideBearing);
            int charAdvance = static_cast<int>(round(advanceWidth * scale));

            if (bitmap) {
                int drawX_base = currentX + static_cast<int>(round(leftSideBearing * scale)) + xoff;
                int drawY_base = currentY + yoff;

                for (int y = 0; y < char_h; ++y) {
                    for (int x = 0; x < char_w; ++x) {
                        int outX = drawX_base + x;
                        int outY = drawY_base + y;
                        if (outX >= 0 && outX < imgWidth && outY >= 0 && outY < imgHeight) {
                            unsigned char alpha = bitmap[y * char_w + x];
                            if (alpha > 10) {
                                size_t pixelIndex = (static_cast<size_t>(outY) * imgWidth + outX) * OUTPUT_CHANNELS;
                                const unsigned char* finalColor = (scheme == ColorScheme::COLOR_ON_WHITE || scheme == ColorScheme::COLOR_ON_BLACK) ? charInfo.color : fgColor;
                                float alphaF = alpha / 255.0f;
                                outputImageData[pixelIndex]     = static_cast<unsigned char>(finalColor[0] * alphaF + outputImageData[pixelIndex]     * (1.0f - alphaF));
                                outputImageData[pixelIndex + 1] = static_cast<unsigned char>(finalColor[1] * alphaF + outputImageData[pixelIndex + 1] * (1.0f - alphaF));
                                outputImageData[pixelIndex + 2] = static_cast<unsigned char>(finalColor[2] * alphaF + outputImageData[pixelIndex + 2] * (1.0f - alphaF));
                            }
                        }
                    }
                }
                stbtt_FreeBitmap(bitmap, nullptr);
            }
             currentX += charAdvance;
        }
        currentY += lineHeight;
    }
}


// 9. 保存图像 (不变)
bool saveImage(const path& outputPath, int width, int height, int channels, const vector<unsigned char>& data) {
    size_t expectedSize = static_cast<size_t>(width) * height * channels;
    if (data.size() != expectedSize) {
         cerr << "Error: Data size passed to saveImage (" << data.size()
              << ") does not match expected size (" << expectedSize
              << "). W=" << width << " H=" << height << " C=" << channels << endl;
         return false;
    }
    if (width <= 0 || height <= 0) {
         cerr << "Error: Invalid width or height passed to saveImage (" << width << "x" << height << ")" << endl;
         return false;
    }

    if (!stbi_write_png(outputPath.string().c_str(), width, height, channels, data.data(), width * channels)) {
        cerr << "Error: Failed to save image to '" << outputPath.string() << "'" << endl;
        return false;
    }
    return true;
}

// --- 新增：处理单个图像文件的函数 ---
bool processImageFile(const path& imagePath,
                      const path& baseOutputDir, // 输出子目录应在此目录下创建
                      const Config& config,
                      const stbtt_fontinfo& fontInfo, // 预加载的字体信息
                      const vector<unsigned char>& fontBuffer) // 字体数据，保持有效
{
    cout << "\n==================================================" << endl;
    cout << "Processing image: " << imagePath.string() << endl;
    cout << "==================================================" << endl;

    // --- 步骤 2 (修改): 设置此图片的输出子目录 ---
    path outputSubDirPath = setupImageOutputSubdirectory(baseOutputDir, imagePath, config.imageOutputSubDirSuffix);
    if (outputSubDirPath.empty()) {
         cerr << "Skipping image due to output directory setup failure." << endl;
         return false; // 检查设置是否失败
    }

    // --- 步骤 3：加载图像 ---
    cout << "Loading image..." << endl;
    auto load_start = high_resolution_clock::now();
    int width, height;
    auto imgDataPtr = loadImage(imagePath.string(), width, height); // unique_ptr 管理内存
    auto load_end = high_resolution_clock::now();
    auto load_duration = duration_cast<milliseconds>(load_end - load_start);
    cout << "-> Image loaded (" << width << "x" << height << "), Time: "
         << fixed << setprecision(3) << load_duration.count() / 1000.0 << " seconds" << endl;
    if (!imgDataPtr) {
        cerr << "Skipping image due to loading failure." << endl;
        return false;
    }


    // --- 启动此图像处理计时器 ---
    auto image_processing_start = high_resolution_clock::now();

    // --- 步骤 4：生成 ASCII 数据 ---
    cout << "Generating ASCII data..." << endl;
    auto ascii_conv_start = high_resolution_clock::now();
    int targetHeight = static_cast<int>(round(static_cast<double>(height * config.targetWidth) / (width * config.charAspectRatioCorrection)));
    targetHeight = max(1, targetHeight);
    cout << "Calculated ASCII grid: " << config.targetWidth << "x" << targetHeight << endl;

    vector<vector<CharColorInfo>> asciiResultData = generateAsciiData(
        imgDataPtr.get(), width, height, config.targetWidth, targetHeight);
    auto ascii_conv_end = high_resolution_clock::now();
    if (asciiResultData.empty()) {
        cerr << "Error: Failed to generate ASCII data for this image. Skipping." << endl;
        return false;
    }
    auto ascii_conv_duration = duration_cast<milliseconds>(ascii_conv_end - ascii_conv_start);
    cout << "-> ASCII data conversion time: " << fixed << setprecision(3) << ascii_conv_duration.count() / 1000.0 << " seconds" << endl;


    // --- 步骤 6：计算尺寸 ---
    int charWidth, lineHeight, outputImageWidth, outputImageHeight, ascent, descent, lineGap;
    float scale;
    calculateOutputDimensions(fontInfo, config.fontSize, config.targetWidth, targetHeight,
                              charWidth, lineHeight, outputImageWidth, outputImageHeight,
                              scale, ascent, descent, lineGap);
    if (outputImageWidth <= 0 || outputImageHeight <= 0) {
        cerr << "Error: Calculated output image dimensions are invalid (" << outputImageWidth << "x" << outputImageHeight << "). Skipping image." << endl;
        return false;
    }
     cout << "Calculated output image size: " << outputImageWidth << "x" << outputImageHeight << endl;


    // --- 步骤 7：处理每个选定的方案 ---
    cout << "Processing color schemes for this image:" << endl;
    bool allSchemesSuccessful = true;
    for (const auto& currentScheme : config.schemesToGenerate) {
        auto theme_start = high_resolution_clock::now();

        string schemeSuffix = getSchemeSuffix(currentScheme);
        // 使用原始图片的文件名（无扩展名）作为基础，加上颜色后缀
        string baseNameForOutput = imagePath.stem().string() + schemeSuffix;
        string currentOutputFilename = baseNameForOutput + config.outputExtension;
        path finalOutputPathObj = outputSubDirPath / currentOutputFilename;

        cout << "  Processing scheme: " << schemeSuffix.substr(1)
             << ", Outputting to: " << finalOutputPathObj.string() << endl;

        // a. 设置颜色
        unsigned char bgColor[3];
        unsigned char fgColor[3];
        setSchemeColors(currentScheme, bgColor, fgColor);

        // b. 为此方案创建缓冲区
        vector<unsigned char> outputImageData;
        try {
            size_t required_size = static_cast<size_t>(outputImageWidth) * outputImageHeight * OUTPUT_CHANNELS;
             if (outputImageWidth > 0 && outputImageHeight > 0 && required_size / outputImageWidth / outputImageHeight != OUTPUT_CHANNELS) {
                 throw std::overflow_error("Image dimensions are too large, causing size calculation overflow.");
             }
             if (required_size == 0 && (outputImageWidth > 0 || outputImageHeight > 0)) {
                 throw std::runtime_error("Calculated image size is zero unexpectedly.");
             }
             if (required_size > outputImageData.max_size()) {
                  throw std::runtime_error("Required image buffer size exceeds vector's maximum capacity.");
             }
             outputImageData.resize(required_size);
        } catch (const std::bad_alloc& e) {
            cerr << "Error: Failed to allocate output image buffer (Size: " << outputImageWidth << "x" << outputImageHeight << "). Out of memory? Skipping scheme." << endl;
            cerr << e.what() << endl;
            allSchemesSuccessful = false;
            continue; // 跳过此主题
        } catch (const std::exception& e) {
             cerr << "Error: Exception occurred while preparing output image buffer: " << e.what() << ". Skipping scheme." << endl;
             allSchemesSuccessful = false;
             continue; // 跳过此主题
        }

        // c. 渲染艺术
        auto render_start = high_resolution_clock::now();
        renderAsciiArt(outputImageData, asciiResultData, fontInfo, currentScheme,
                       bgColor, fgColor, outputImageWidth, outputImageHeight,
                       ascent, scale, lineHeight);
        auto render_end = high_resolution_clock::now();
        auto render_duration = duration_cast<milliseconds>(render_end - render_start);
        cout << "    -> Rendering time: " << fixed << setprecision(3) << render_duration.count() / 1000.0 << " seconds" << endl;

        // d. 保存图像
         auto save_start = high_resolution_clock::now();
        if (!saveImage(finalOutputPathObj, outputImageWidth, outputImageHeight, OUTPUT_CHANNELS, outputImageData)) {
            cerr << "  Error: Failed to save scheme " << schemeSuffix.substr(1) << "." << endl;
            allSchemesSuccessful = false;
            // 决定是继续还是停止：continue;
        } else {
            auto save_end = high_resolution_clock::now();
            auto save_duration = duration_cast<milliseconds>(save_end - save_start);
            cout << "    -> Saving time: " << fixed << setprecision(3) << save_duration.count() / 1000.0 << " seconds" << endl;
            cout << "  Scheme " << schemeSuffix.substr(1) << " saved successfully." << endl;
        }

        auto theme_end = high_resolution_clock::now();
        auto theme_duration = duration_cast<milliseconds>(theme_end - theme_start);
         // cout << "  -> Total processing time for this theme: " << fixed << setprecision(3) << theme_duration.count() / 1000.0 << " seconds" << endl;

    } // --- 方案循环结束 ---

    auto image_processing_end = high_resolution_clock::now();
    auto image_processing_duration = duration_cast<milliseconds>(image_processing_end - image_processing_start);
    cout << "-> Finished processing image '" << imagePath.filename().string() << "'. Total time: "
         << fixed << setprecision(3) << image_processing_duration.count() / 1000.0 << " seconds" << endl;

    return allSchemesSuccessful; // 如果所有方案都成功则返回 true
}


// --- 主应用程序逻辑 ---
int main(int argc, char* argv[]) {
    Config config; // 使用默认值初始化 Config 对象
    const string configFilename = "config.txt";

    // --- 确定可执行文件路径和配置文件路径 ---
    path exePath;
    if (argc > 0 && argv[0] != nullptr) {
         try {
             exePath = filesystem::path(argv[0]);
         } catch (const filesystem::filesystem_error& e) {
             cerr << "Warning: Failed to resolve executable path '" << argv[0] << "': " << e.what() << endl;
             cerr << "Attempting to use current working directory as fallback." << endl;
             exePath = filesystem::current_path() / (argv[0] ? filesystem::path(argv[0]).filename() : "");
         }
    } else {
         cerr << "Warning: Could not get executable path (argc=" << argc << "). Attempting to use current working directory." << endl;
         try {
             exePath = filesystem::current_path() / "unknown_executable";
         } catch (const filesystem::filesystem_error& e) {
             cerr << "Error: Failed to get current working directory: " << e.what() << endl;
             return 1;
         }
    }
    path exeDir = exePath.parent_path();
    path configPathObj = exeDir / configFilename;

    // --- 加载配置 ---
    loadConfiguration(configPathObj, config); // 传递 Config 对象引用
    config.finalFontPath = (exeDir / config.fontFilename).string(); // 计算最终字体路径

    // --- 打印最终使用的配置 ---
    cout << "\n--- Using Configuration ---" << endl;
    cout << "Target Width: " << config.targetWidth << endl;
    cout << "Aspect Ratio Correction: " << config.charAspectRatioCorrection << endl;
    cout << "Font Filename: " << config.fontFilename << endl;
    cout << "Font Size: " << config.fontSize << endl;
    cout << "--------------------------" << endl;

    // --- 加载字体 (一次性) ---
    stbtt_fontinfo fontInfo;
    vector<unsigned char> fontBuffer; // 必须在 fontInfo 的作用域内保持有效
    if (!loadFontInfo(config.finalFontPath, fontInfo, fontBuffer)) {
        cerr << "Error loading font: " << config.finalFontPath << endl;
        cerr << "Please ensure the font file '" << config.fontFilename << "' (specified in config or default) is in the same directory as the executable." << endl;
        return 1;
    }

    // --- 获取输入路径 ---
    string inputPathStr = getUserInputPath();
    if (inputPathStr.empty()) return 1;
    path inputPath(inputPathStr);

    int processedCount = 0;
    int failedCount = 0;
    auto overall_start_time = high_resolution_clock::now();

    // --- 检查输入路径类型 ---
    try {
        if (!filesystem::exists(inputPath)) {
            cerr << "Error: Input path does not exist: " << inputPath.string() << endl;
            return 1;
        }

        if (filesystem::is_regular_file(inputPath)) {
            // --- 处理单个文件 ---
            cout << "\nInput is a single file. Processing..." << endl;
            if (isImageFile(inputPath)) {
                path baseOutputDir = inputPath.parent_path(); // 输出到图片所在目录
                if (processImageFile(inputPath, baseOutputDir, config, fontInfo, fontBuffer)) {
                    processedCount++;
                } else {
                    failedCount++;
                }
            } else {
                 cerr << "Error: Input file is not a supported image type: " << inputPath.string() << endl;
                 failedCount++;
            }

        } else if (filesystem::is_directory(inputPath)) {
            // --- 处理文件夹 ---
            cout << "\nInput is a directory. Processing image files inside..." << endl;

            // 创建主输出目录 (在输入目录旁边)
            path batchBaseOutputDir = inputPath.parent_path() / (inputPath.filename().string() + config.batchOutputSubDirSuffix);
            try {
                 if(filesystem::create_directories(batchBaseOutputDir)) {
                     cout << "Created main batch output directory: " << batchBaseOutputDir.string() << endl;
                 } else if (!filesystem::is_directory(batchBaseOutputDir)) {
                     cerr << "Error: Failed to create or access main batch output directory: " << batchBaseOutputDir.string() << endl;
                     return 1; // 无法创建主输出目录则退出
                 }
            } catch (const filesystem::filesystem_error& e) {
                 cerr << "Error creating main batch output directory: " << batchBaseOutputDir.string() << " - " << e.what() << endl;
                 return 1;
            }


            filesystem::directory_iterator dir_iter(inputPath);
            filesystem::directory_iterator end_iter; // 默认构造为结束迭代器

            for (; dir_iter != end_iter; ++dir_iter) {
                const auto& entry = *dir_iter;
                if (entry.is_regular_file() && isImageFile(entry.path())) {
                    // 对于文件夹中的每个图片，都在 batchBaseOutputDir 中处理
                    if (processImageFile(entry.path(), batchBaseOutputDir, config, fontInfo, fontBuffer)) {
                         processedCount++;
                    } else {
                         failedCount++;
                    }
                }
                // 可以选择性地忽略非图像文件或子目录
                // else {
                //     if (!entry.is_directory()) { // 不报告目录本身
                //          cout << "Skipping non-image file: " << entry.path().filename().string() << endl;
                //     }
                // }
            }
             if (processedCount == 0 && failedCount == 0) {
                 cout << "No supported image files found in the directory: " << inputPath.string() << endl;
             }

        } else {
            cerr << "Error: Input path is neither a regular file nor a directory: " << inputPath.string() << endl;
            return 1;
        }

    } catch (const filesystem::filesystem_error& e) {
         cerr << "Filesystem error accessing path '" << inputPath.string() << "': " << e.what() << endl;
         return 1;
    } catch (const std::exception& e) {
         cerr << "An unexpected error occurred: " << e.what() << endl;
         return 1;
    }

    auto overall_end_time = high_resolution_clock::now();
    auto overall_duration = duration_cast<seconds>(overall_end_time - overall_start_time);

    cout << "\n==================================================" << endl;
    cout << "Overall Processing Summary:" << endl;
    cout << "  Successfully processed: " << processedCount << " image(s)" << endl;
    cout << "  Failed/Skipped:       " << failedCount << " image(s)" << endl;
    cout << "  Total time elapsed:   " << overall_duration.count() << " seconds" << endl;
    cout << "==================================================" << endl;

    cin.get();
    return (failedCount > 0); // 如果有失败则返回非零
}
