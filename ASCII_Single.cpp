//gdb Launch ASCII.exe(Hascii)
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
#include <stdexcept>    // For runtime_error in PDF generation

// --- Add libHaru Header ---
#ifdef _WIN32
#include <windows.h> // Required by hpdf.h on Windows? Check documentation if issues arise.
#endif
#include <hpdf.h> // <-- Include libHaru header

// Define STB_IMAGE_IMPLEMENTATION 的实现
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Define stb_image_write 的实现
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Define stb_truetype 的实现
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"


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

// PDF Points per inch (standard definition)
const float POINTS_PER_INCH = 72.0f;

enum class ColorScheme {
    // ... (保持不变) ...
    AMBER_ON_BLACK,   // 琥珀色字体，黑色背景
    BLACK_ON_YELLOW,// 黑色字体，黄色背景
    BLACK_ON_CYAN,// 黑色字体，青色背景
    COLOR_ON_WHITE,//彩色字体，白色背景，
    COLOR_ON_BLACK,//彩色字体，白色背景
    CYAN_ON_BLACK,// 青色字体，黑色背景
    GRAY_ON_BLACK,// 灰色字体，黑色背景
    GREEN_ON_BLACK,//绿色字体，黑色背景
    MAGENTA_ON_BLACK,//洋红字体，黑色背景
    PURPLE_ON_BLACK,//紫色字体，黑色背景
    SEPIA,// 深褐色调 (模拟旧照片)
    SOLARIZED_DARK,//灰青字体，深蓝灰背景
    SOLARIZED_LIGHT,//深灰字体，米白底背景
    WHITE_ON_BLACK,//白色字体，黑色背景
    WHITE_ON_BLUE,// 白色字体，蓝色背景
    WHITE_ON_DARK_RED,//白色字体，深红背景
    YELLOW_ON_BLACK, // 黄色字体，黑色背景
    BLACK_ON_WHITE, //黑色字体，白色背景
};

struct CharColorInfo {
    char character;
    unsigned char color[3]; // 来自原始图像的 R, G, B 值
};

// --- 配置 ---
struct Config {
    int targetWidth = 1024;
    double charAspectRatioCorrection = 2.0;
    string fontFilename = "Consolas.ttf";
    float fontSize = 15.0f; // Font size in pixels for PNG rendering
    string finalFontPath = ""; // 将由 main 计算

    // --- PDF Specific Config ---
    float pdfFontSize = 10.0f; // Font size in points for PDF output

    // --- 新增: 控制 PDF 输出 ---
    // 在此处修改：设置为 true 以生成 PDF，设置为 false 以禁用 PDF 生成
    bool generatePdfOutput = false;

    // --- 其他处理相关的常量 ---
    string baseOutputFilename = "output_ascii_art";
    string outputPngExtension = ".png"; // Explicitly name PNG extension
    string outputPdfExtension = ".pdf"; // PDF extension
    string imageOutputSubDirSuffix = "_ascii_output";
    string batchOutputSubDirSuffix = "_ascii_batch_output";

    // --- 选择要默认生成的颜色搭配 ---
    vector<ColorScheme> schemesToGenerate = {
        // ... (保持不变) ...
        ColorScheme::BLACK_ON_WHITE,//黑色字体，白色背景
        ColorScheme::COLOR_ON_WHITE,//彩色字体，白色背景，
    };
};


// --- libHaru Error Handler ---
#ifdef HPDF_DLL
void __stdcall
#else
void
#endif
error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void *user_data) {
    // Use cerr for errors
    cerr << "PDF ERROR: error_no=" << error_no << ", detail_no=" << detail_no << endl;
    // You might want to throw an exception or set a flag here
    // For simplicity, we'll just print the error. In robust code, handle this better.
    // throw runtime_error("libHaru PDF generation error."); // Example
}


// --- 辅助函数 ---

// 去除字符串首尾空格的辅助函数 (不变)
string trim(const string& str) {
    // ... (保持不变) ...
    size_t first = str.find_first_not_of(" \t\n\r\f\v");
    if (string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(first, (last - first + 1));
}

// 检查文件扩展名是否为支持的图像格式 (不变)
bool isImageFile(const path& p) {
    // ... (保持不变) ...
     if (!p.has_extension()) {
        return false;
    }
    string ext = p.extension().string();
    transform(ext.begin(), ext.end(), ext.begin(), ::tolower); // 转为小写
    return SUPPORTED_EXTENSIONS.count(ext);
}

// 将文件读入字节向量 (不变)
vector<unsigned char> read_file(const string& filename) {
    // ... (保持不变) ...
    ifstream file(filename, ios::binary | ios::ate);
    if (!file) {
        cerr << "Error: Cannot open file '" << filename << "'" << endl;
        return {};
    }
    streamsize size = file.tellg();
    file.seekg(0, ios::beg);
    vector<unsigned char> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        cerr << "Error: Cannot read file '" << filename << "'" << endl;
        return {};
    }
    return buffer;
}

// 根据颜色方案获取用于文件名的描述性后缀 (不变)
string getSchemeSuffix(ColorScheme scheme) {
    // ... (保持不变) ...
     switch (scheme) {
        case ColorScheme::BLACK_ON_WHITE:   return "_BlackOnWhite";
        case ColorScheme::WHITE_ON_BLACK:   return "_WhiteOnBlack";
        case ColorScheme::GREEN_ON_BLACK:   return "_GreenOnBlack";
        case ColorScheme::PURPLE_ON_BLACK:  return "_PurpleOnBlack";
        case ColorScheme::COLOR_ON_WHITE:   return "_ColorOnWhite";
        case ColorScheme::COLOR_ON_BLACK:   return "_ColorOnBlack";
        case ColorScheme::AMBER_ON_BLACK:   return "_AmberOnBlack";
        case ColorScheme::CYAN_ON_BLACK:    return "_CyanOnBlack";
        case ColorScheme::YELLOW_ON_BLACK:  return "_YellowOnBlack";
        case ColorScheme::WHITE_ON_BLUE:    return "_WhiteOnBlue";
        case ColorScheme::BLACK_ON_YELLOW:  return "_BlackOnYellow";
        case ColorScheme::BLACK_ON_CYAN:    return "_BlackOnCyan";
        case ColorScheme::SEPIA:            return "_Sepia";
        case ColorScheme::MAGENTA_ON_BLACK: return "_MagentaOnBlack";
        case ColorScheme::WHITE_ON_DARK_RED: return "_WhiteOnDarkRed";
        case ColorScheme::GRAY_ON_BLACK:    return "_GrayOnBlack";
        case ColorScheme::SOLARIZED_DARK:   return "_SolarizedDark";
        case ColorScheme::SOLARIZED_LIGHT:  return "_SolarizedLight";
        default:                            return "_UnknownScheme";
    }
}


// --- 配置加载函数 (Add pdfFontSize) ---
// 注意：此函数现在不会读取 generatePdfOutput，因为它是在代码中设置的
bool loadConfiguration(const path& configPath, Config& config)
{
    ifstream configFile(configPath);
    if (!configFile) {
        cout << "Info: Configuration file '" << configPath.string() << "' not found. Using default settings." << endl;
        return true;
    }

    cout << "Info: Loading configuration from '" << configPath.string() << "'..." << endl;
    string line;
    int lineNumber = 0;
    bool loadedSuccessfully = true;

    while (getline(configFile, line)) {
        lineNumber++;
        line = trim(line);

        if (line.empty() || line[0] == '#') continue;

        size_t separatorPos = line.find('=');
        if (separatorPos == string::npos) {
            cerr << "Warning: Invalid format in config file at line " << lineNumber << ": Missing '=' separator. Line: \"" << line << "\"" << endl;
            loadedSuccessfully = false;
            continue;
        }

        string key = trim(line.substr(0, separatorPos));
        string value = trim(line.substr(separatorPos + 1));

        try { // Use a single try-catch for parsing attempts
            if (key == "targetWidth") {
                config.targetWidth = stoi(value);
                cout << "  -> Loaded targetWidth = " << config.targetWidth << endl;
            } else if (key == "charAspectRatioCorrection") {
                config.charAspectRatioCorrection = stod(value);
                cout << "  -> Loaded charAspectRatioCorrection = " << config.charAspectRatioCorrection << endl;
            } else if (key == "fontFilename") {
                if (!value.empty()) {
                    config.fontFilename = value;
                    cout << "  -> Loaded fontFilename = " << config.fontFilename << endl;
                } else {
                     cerr << "Warning: Empty value for 'fontFilename' in config file at line " << lineNumber << ". Using default." << endl;
                     loadedSuccessfully = false;
                }
            } else if (key == "fontSize") { // This is the PNG font size
                config.fontSize = stof(value);
                cout << "  -> Loaded fontSize (for PNG) = " << config.fontSize << endl;
            } else if (key == "pdfFontSize") { // New config key for PDF
                config.pdfFontSize = stof(value);
                cout << "  -> Loaded pdfFontSize (for PDF) = " << config.pdfFontSize << endl;
            } else {
                cout << "Info: Unknown key '" << key << "' in config file at line " << lineNumber << ". Ignoring." << endl;
            }
        } catch (const std::invalid_argument& e) {
           cerr << "Warning: Invalid value for '" << key << "' in config file at line " << lineNumber << ". Using default. Value: \"" << value << "\"" << endl;
           loadedSuccessfully = false;
        } catch (const std::out_of_range& e) {
           cerr << "Warning: Value out of range for '" << key << "' in config file at line " << lineNumber << ". Using default. Value: \"" << value << "\"" << endl;
           loadedSuccessfully = false;
        }
    }

    configFile.close();
    return loadedSuccessfully;
}


// --- 核心功能模块 ---

// 1. 获取用户输入 (不变)
string getUserInputPath() {
    // ... (保持不变) ...
     string inputPathStr;
    cout << "Please enter the image file or folder path: ";
    getline(cin, inputPathStr);
    if (inputPathStr.empty()) {
        cerr << "Error: Input path is empty." << endl;
    }
    return inputPathStr;
}

// 2. 设置单个图片的输出子目录 (不变)
path setupImageOutputSubdirectory(const path& baseOutputDir, const path& imagePath, const string& suffix) {
    // ... (保持不变) ...
    string subDirName = imagePath.stem().string() + suffix;
    path outputSubDirPath = baseOutputDir / subDirName;
    try {
        if (filesystem::create_directories(outputSubDirPath)) {
            cout << "Created output subdirectory: " << outputSubDirPath.string() << endl;
        } else {
             if (!filesystem::exists(outputSubDirPath)) {
                 cerr << "Error: Failed to create output subdirectory: " << outputSubDirPath.string() << endl;
                 return path();
             } else if (!filesystem::is_directory(outputSubDirPath)) {
                 cerr << "Error: Output path exists but is not a directory: " << outputSubDirPath.string() << endl;
                 return path();
             }
        }
        return outputSubDirPath;
    } catch (const filesystem::filesystem_error& e) {
        cerr << "Error: Filesystem error occurred while creating/accessing output subdirectory: " << outputSubDirPath.string() << endl;
        cerr << "Details: " << e.what() << endl;
        if (!filesystem::is_directory(outputSubDirPath)) return path();
        cerr << "Warning: Attempting to continue, assuming directory exists..." << endl;
        return outputSubDirPath;
    } catch (const std::exception& e) {
        cerr << "Error: An unknown exception occurred while creating directory: " << e.what() << endl;
        return path();
    }
}


// 3. 加载输入图像 (不变)
unique_ptr<unsigned char, void(*)(void*)> loadImage(const string& imagePath, int& width, int& height) {
   // ... (保持不变) ...
  unsigned char *data = stbi_load(imagePath.c_str(), &width, &height, nullptr, OUTPUT_CHANNELS);
    if (data == nullptr) {
        cerr << "Error: Failed to load image '" << imagePath << "' (or convert to RGB)" << endl;
        cerr << "STB Image Error Reason: " << stbi_failure_reason() << endl;
        return unique_ptr<unsigned char, void(*)(void*)>(nullptr, stbi_image_free);
    }
    return unique_ptr<unsigned char, void(*)(void*)>(data, stbi_image_free);
}

// 4. 生成 ASCII 数据 (不变)
vector<vector<CharColorInfo>> generateAsciiData(const unsigned char* imgData, int width, int height, int targetWidth, int targetHeight)
{
    // ... (保持不变) ...
    vector<vector<CharColorInfo>> asciiResultData;
    if (!imgData || width <= 0 || height <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        cerr << "Error: Invalid arguments passed to generateAsciiData." << endl;
        return asciiResultData;
    }

    asciiResultData.reserve(targetHeight);
    double xScale = static_cast<double>(width) / targetWidth;
    double yScale = static_cast<double>(height) / targetHeight;

    for (int yOut = 0; yOut < targetHeight; ++yOut) {
        vector<CharColorInfo> currentLineData;
        currentLineData.reserve(targetWidth);
        for (int xOut = 0; xOut < targetWidth; ++xOut) {
            int xImg = static_cast<int>(std::floor((xOut + 0.5) * xScale)); // Use std::floor
            int yImg = static_cast<int>(std::floor((yOut + 0.5) * yScale)); // Use std::floor
            xImg = max(0, min(xImg, width - 1));
            yImg = max(0, min(yImg, height - 1));

            size_t pixelOffset = (static_cast<size_t>(yImg) * width + xImg) * OUTPUT_CHANNELS;
            unsigned char r = imgData[pixelOffset];
            unsigned char g = imgData[pixelOffset + 1];
            unsigned char b = imgData[pixelOffset + 2];

            // --- Optional: Use luminance for better perceptual brightness ---
            // float grayF = 0.299f * r + 0.587f * g + 0.114f * b;
            // int gray = static_cast<int>(grayF);
            // --- Simple average (current method) ---
             int gray = (static_cast<int>(r) + g + b) / 3;


            int asciiIndex = static_cast<int>(std::floor((gray / 256.0f) * NUM_ASCII_CHARS)); // Use std::floor
            asciiIndex = max(0, min(asciiIndex, NUM_ASCII_CHARS - 1));

            CharColorInfo info;
            info.character = ASCII_CHARS[static_cast<size_t>(asciiIndex)];
            info.color[0] = r; info.color[1] = g; info.color[2] = b;
            currentLineData.push_back(info);
        }
        asciiResultData.push_back(currentLineData);
    }
    return asciiResultData;
}


// 5. 加载字体信息 (不变, 但注意 finalFontPath 会被 PDF 使用)
bool loadFontInfo(const string& fontPath, stbtt_fontinfo& fontInfo, vector<unsigned char>& fontBuffer) {
    // ... (保持不变) ...
    cout << "\nLoading font file: " << fontPath << " ..." << endl;
    fontBuffer = read_file(fontPath);
    if (fontBuffer.empty()) return false;

    if (!stbtt_InitFont(&fontInfo, fontBuffer.data(), stbtt_GetFontOffsetForIndex(fontBuffer.data(), 0))) {
        cerr << "Error: Failed to initialize font." << endl;
        return false;
    }

    // --- Font name retrieval (remains the same) ---
     string fontName = "Name not found";
    int length = 0;
    const char* namePtr = stbtt_GetFontNameString(&fontInfo, &length,
                                                STBTT_PLATFORM_ID_MICROSOFT,
                                                STBTT_MS_EID_UNICODE_BMP,
                                                STBTT_MS_LANG_ENGLISH,
                                                4); // Name ID 4: Full font name

    if (namePtr && length > 0) {
        string tempName;
        tempName.reserve(length / 2 + 1);
        for (int i = 0; i < length; i += 2) {
            if (namePtr[i] == 0 && namePtr[i+1] != 0) {
                tempName += namePtr[i+1];
            } else {
                 if (namePtr[i] != 0 || namePtr[i+1] != 0)
                    tempName += '?';
            }
        }
         if (!tempName.empty()) {
             fontName = tempName;
         } else {
             namePtr = stbtt_GetFontNameString(&fontInfo, &length, STBTT_PLATFORM_ID_MAC, STBTT_MAC_EID_ROMAN, STBTT_MAC_LANG_ENGLISH, 1);
             if (namePtr && length > 0) {
                 fontName = string(namePtr, length);
             }
        }
    } else {
         namePtr = stbtt_GetFontNameString(&fontInfo, &length, STBTT_PLATFORM_ID_MAC, STBTT_MAC_EID_ROMAN, STBTT_MAC_LANG_ENGLISH, 1);
         if (namePtr && length > 0) {
             fontName = string(namePtr, length);
         }
    }

    cout << "Font loaded successfully." << endl;
    cout << "  -> Font Location: " << fontPath << endl;
    cout << "  -> Font Name: " << fontName << endl;
    return true;
}


// 6. 计算输出尺寸和字体度量 (基本不变, 但区分 PNG 和 PDF)
// Note: This function calculates pixel dimensions for PNG. PDF dimensions are calculated differently.
void calculateOutputDimensions(const stbtt_fontinfo& fontInfo, float pngFontSize, int targetWidth, int targetHeight,
                                int& charWidthPx, int& lineHeightPx, int& outputImageWidthPx, int& outputImageHeightPx,
                                float& scaleForPng, int& ascentPx, int& descentPx, int& lineGapPx)
{
    scaleForPng = stbtt_ScaleForPixelHeight(&fontInfo, pngFontSize);
    stbtt_GetFontVMetrics(&fontInfo, &ascentPx, &descentPx, &lineGapPx);
    ascentPx = static_cast<int>(std::round(ascentPx * scaleForPng)); // Use std::round
    descentPx = static_cast<int>(std::round(descentPx * scaleForPng)); // Use std::round
    lineGapPx = static_cast<int>(std::round(lineGapPx * scaleForPng)); // Use std::round
    lineHeightPx = ascentPx - descentPx + lineGapPx;

    // Estimate character width in pixels using 'M'
    int advanceWidth, leftSideBearing;
    stbtt_GetCodepointHMetrics(&fontInfo, 'M', &advanceWidth, &leftSideBearing);
    // Use float for potentially more accurate calculation before rounding
    float charWidthFloat = advanceWidth * scaleForPng;
    charWidthPx = static_cast<int>(std::round(charWidthFloat)); // Use std::round

    if (charWidthPx <= 0) {
        cerr << "Warning: Calculated PNG character width is zero or negative. Using default value 1." << endl;
        charWidthPx = 1;
    }
     if (lineHeightPx <= 0) {
         cerr << "Warning: Calculated PNG line height is zero or negative. Using default value 1." << endl;
        lineHeightPx = 1; // Ensure positive line height
    }


    outputImageWidthPx = targetWidth * charWidthPx;
    outputImageHeightPx = targetHeight * lineHeightPx;

    outputImageWidthPx = max(1, outputImageWidthPx);
    outputImageHeightPx = max(1, outputImageHeightPx);
}


// 7. 设置配色方案颜色 (不变)
void setSchemeColors(ColorScheme scheme, unsigned char bgColor[3], unsigned char fgColor[3]) {
    // ... (保持不变) ...
     switch (scheme) {
        case ColorScheme::AMBER_ON_BLACK:
            bgColor[0] = 0x00; bgColor[1] = 0x00; bgColor[2] = 0x00; // #000000 (Black)
            fgColor[0] = 0xFF; fgColor[1] = 0xBF; fgColor[2] = 0x00; // #FFBF00 (Amber)
            break;
        case ColorScheme::BLACK_ON_YELLOW:
            bgColor[0] = 0xFF; bgColor[1] = 0xFF; bgColor[2] = 0xAA; // #FFFFAA (Light Yellow)
            fgColor[0] = 0x00; fgColor[1] = 0x00; fgColor[2] = 0x00; // #000000 (Black)
            break;
        case ColorScheme::BLACK_ON_CYAN:
            bgColor[0] = 0xAA; bgColor[1] = 0xFF; bgColor[2] = 0xFF; // #AAFFFF (Light Cyan)
            fgColor[0] = 0x00; fgColor[1] = 0x00; fgColor[2] = 0x00; // #000000 (Black)
            break;
        case ColorScheme::COLOR_ON_WHITE:
            bgColor[0] = 0xC8; bgColor[1] = 0xC8; bgColor[2] = 0xC8; // #C8C8C8 (Light Gray)
            fgColor[0] = 0x00; fgColor[1] = 0x00; fgColor[2] = 0x00; // Not used directly
            break;
        case ColorScheme::COLOR_ON_BLACK:
            bgColor[0] = 0x36; bgColor[1] = 0x36; bgColor[2] = 0x36; // #363636 (Dark Gray)
            fgColor[0] = 0xFF; fgColor[1] = 0xFF; fgColor[2] = 0xFF; // Not used directly
            break;
        case ColorScheme::CYAN_ON_BLACK:
            bgColor[0] = 0x00; bgColor[1] = 0x00; bgColor[2] = 0x00; // #000000 (Black)
            fgColor[0] = 0x00; fgColor[1] = 0xFF; fgColor[2] = 0xFF; // #00FFFF (Cyan)
            break;
        case ColorScheme::GRAY_ON_BLACK:
            bgColor[0] = 0x00; bgColor[1] = 0x00; bgColor[2] = 0x00; // #000000 (Black)
            fgColor[0] = 0xAA; fgColor[1] = 0xAA; fgColor[2] = 0xAA; // #AAAAAA (Gray)
            break;
        case ColorScheme::GREEN_ON_BLACK:
            bgColor[0] = 0x00; bgColor[1] = 0x00; bgColor[2] = 0x00; // #000000 (Black)
            fgColor[0] = 0x00; fgColor[1] = 0xFF; fgColor[2] = 0x00; // #00FF00 (Green)
            break;
        case ColorScheme::MAGENTA_ON_BLACK:
            bgColor[0] = 0x00; bgColor[1] = 0x00; bgColor[2] = 0x00; // #000000 (Black)
            fgColor[0] = 0xFF; fgColor[1] = 0x00; fgColor[2] = 0xFF; // #FF00FF (Magenta)
            break;
        case ColorScheme::PURPLE_ON_BLACK:
            bgColor[0] = 0x00; bgColor[1] = 0x00; bgColor[2] = 0x00; // #000000 (Black)
            fgColor[0] = 0x80; fgColor[1] = 0x00; fgColor[2] = 0x80; // #800080 (Purple)
            break;
        case ColorScheme::SEPIA:
            bgColor[0] = 0xF0; bgColor[1] = 0xE6; bgColor[2] = 0x8C; // #F0E68C (Khaki-ish)
            fgColor[0] = 0x70; fgColor[1] = 0x42; fgColor[2] = 0x14; // #704214 (SaddleBrown-ish)
            break;
        case ColorScheme::SOLARIZED_DARK:
            bgColor[0] = 0x00; bgColor[1] = 0x2b; bgColor[2] = 0x36; // #002b36 (Base03)
            fgColor[0] = 0x83; fgColor[1] = 0x94; fgColor[2] = 0x96; // #839496 (Base0)
            break;
        case ColorScheme::SOLARIZED_LIGHT:
            bgColor[0] = 0xfd; bgColor[1] = 0xf6; bgColor[2] = 0xe3; // #fdf6e3 (Base3)
            fgColor[0] = 0x65; fgColor[1] = 0x7b; fgColor[2] = 0x83; // #657b83 (Base00)
            break;
        case ColorScheme::WHITE_ON_BLACK:
            bgColor[0] = 0x00; bgColor[1] = 0x00; bgColor[2] = 0x00; // #000000 (Black)
            fgColor[0] = 0xFF; fgColor[1] = 0xFF; fgColor[2] = 0xFF; // #FFFFFF (White)
            break;
        case ColorScheme::WHITE_ON_BLUE:
            bgColor[0] = 0x00; bgColor[1] = 0x00; bgColor[2] = 0xAA; // #0000AA (Darker Blue)
            fgColor[0] = 0xFF; fgColor[1] = 0xFF; fgColor[2] = 0xFF; // #FFFFFF (White)
            break;
        case ColorScheme::WHITE_ON_DARK_RED:
            bgColor[0] = 0x8B; bgColor[1] = 0x00; bgColor[2] = 0x00; // #8B0000 (Dark Red)
            fgColor[0] = 0xFF; fgColor[1] = 0xFF; fgColor[2] = 0xFF; // #FFFFFF (White)
            break;
        case ColorScheme::YELLOW_ON_BLACK:
            bgColor[0] = 0x00; bgColor[1] = 0x00; bgColor[2] = 0x00; // #000000 (Black)
            fgColor[0] = 0xFF; fgColor[1] = 0xFF; fgColor[2] = 0x00; // #FFFF00 (Yellow)
            break;
        case ColorScheme::BLACK_ON_WHITE:
        default:
            bgColor[0] = 0xC8; bgColor[1] = 0xC8; bgColor[2] = 0xC8; // #C8C8C8 (Light Gray)
            fgColor[0] = 0x00; fgColor[1] = 0x00; fgColor[2] = 0x00; // #000000 (Black)
            break;
    }
}

// 8. 将 ASCII 艺术渲染到图像缓冲区 (PNG) (不变)
void renderAsciiArt(vector<unsigned char>& outputImageData,
                    const vector<vector<CharColorInfo>>& asciiData,
                    const stbtt_fontinfo& fontInfo,
                    ColorScheme scheme,
                    const unsigned char bgColor[3], // 此方案的背景色
                    const unsigned char fgColor[3], // 固定前景色（如果使用）
                    int imgWidth, int imgHeight,
                    int ascentPx, float scaleForPng, int lineHeightPx) // Use pixel metrics
{
    // 首先填充背景
    for (size_t i = 0; i < outputImageData.size(); i += OUTPUT_CHANNELS) {
        outputImageData[i]     = bgColor[0];
        outputImageData[i + 1] = bgColor[1];
        outputImageData[i + 2] = bgColor[2];
    }

    // 渲染字符
    int currentY = ascentPx; // Start rendering at the first baseline (in pixels)
    for (const auto& lineData : asciiData) {
        int currentX = 0;
        for (const auto& charInfo : lineData) {
            char c = charInfo.character;
            int char_w, char_h, xoff, yoff;
            // Get bitmap using the scale calculated for PNG pixel height
            unsigned char* bitmap = stbtt_GetCodepointBitmap(&fontInfo, scaleForPng, scaleForPng, c, &char_w, &char_h, &xoff, &yoff);

            int advanceWidthPx, leftSideBearingPx;
            stbtt_GetCodepointHMetrics(&fontInfo, c, &advanceWidthPx, &leftSideBearingPx);
            // Use the scale for PNG to get pixel advance width
            int charAdvancePx = static_cast<int>(std::round(advanceWidthPx * scaleForPng)); // Use std::round

            if (bitmap) {
                // Calculate draw position in pixels
                int drawX_base = currentX + static_cast<int>(std::round(leftSideBearingPx * scaleForPng)) + xoff; // Use std::round
                int drawY_base = currentY + yoff; // yoff is already scaled by GetCodepointBitmap

                for (int y = 0; y < char_h; ++y) {
                    for (int x = 0; x < char_w; ++x) {
                        int outX = drawX_base + x;
                        int outY = drawY_base + y;
                        if (outX >= 0 && outX < imgWidth && outY >= 0 && outY < imgHeight) {
                            unsigned char alpha = bitmap[y * char_w + x];
                            if (alpha > 10) { // Simple threshold for visibility
                                size_t pixelIndex = (static_cast<size_t>(outY) * imgWidth + outX) * OUTPUT_CHANNELS;
                                const unsigned char* finalColor = (scheme == ColorScheme::COLOR_ON_WHITE || scheme == ColorScheme::COLOR_ON_BLACK) ? charInfo.color : fgColor;

                                float alphaF = alpha / 255.0f;
                                // Alpha blending onto the background color already present
                                outputImageData[pixelIndex]     = static_cast<unsigned char>(finalColor[0] * alphaF + outputImageData[pixelIndex]     * (1.0f - alphaF));
                                outputImageData[pixelIndex + 1] = static_cast<unsigned char>(finalColor[1] * alphaF + outputImageData[pixelIndex + 1] * (1.0f - alphaF));
                                outputImageData[pixelIndex + 2] = static_cast<unsigned char>(finalColor[2] * alphaF + outputImageData[pixelIndex + 2] * (1.0f - alphaF));
                            }
                        }
                    }
                }
                stbtt_FreeBitmap(bitmap, nullptr);
            }
            currentX += charAdvancePx; // Advance by pixel width for PNG
        }
        currentY += lineHeightPx; // Move down by pixel line height for PNG
    }
}


// 9. 保存图像 (PNG) (不变)
bool saveImage(const path& outputPath, int width, int height, int channels, const vector<unsigned char>& data) {
    // ... (保持不变) ...
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

// --- NEW: Function to save ASCII Art as PDF ---
bool savePdf(const path& outputPath,
             const vector<vector<CharColorInfo>>& asciiData,
             const Config& config, // Need font path, pdf font size
             ColorScheme scheme,
             const unsigned char bgColorUint[3], // Background color 0-255
             const unsigned char fgColorUint[3]) // Default foreground color 0-255
{
    HPDF_Doc pdf;
    HPDF_Page page;
    HPDF_Font pdfFont;
    const char *fontNameCStr; // Font name within PDF

    // --- Create PDF Document ---
    pdf = HPDF_New(error_handler, nullptr);
    if (!pdf) {
        cerr << "PDF ERROR: Cannot create HPDF_Doc object." << endl;
        return false;
    }

    try { // Use try-catch for easier cleanup on error with HPDF_FreeDoc
        // --- Load TrueType Font ---
        // Use the same font file specified in the config
        // The third parameter 'HPDF_TRUE' attempts to embed the font.
        fontNameCStr = HPDF_LoadTTFontFromFile(pdf, config.finalFontPath.c_str(), HPDF_TRUE);
        if (!fontNameCStr) {
            cerr << "PDF Error: Cannot load font from file: " << config.finalFontPath << endl;
            // Attempt to fall back to a standard font
            cerr << "PDF Info: Falling back to Helvetica." << endl;
            fontNameCStr = "Helvetica"; // Or another standard PDF font
        }
         pdfFont = HPDF_GetFont(pdf, fontNameCStr, nullptr); // Use default encoding (e.g., WinAnsi)
        if (!pdfFont) {
            cerr << "PDF Error: Cannot get font: " << fontNameCStr << endl;
            throw runtime_error("Failed to get PDF font."); // Trigger cleanup
        }


        // --- Add Page ---
        page = HPDF_AddPage(pdf);
        if (!page) {
            cerr << "PDF Error: Cannot add page." << endl;
            throw runtime_error("Failed to add PDF page."); // Trigger cleanup
        }

        // --- Calculate Page Dimensions in Points ---
        // We need character width and line height in *points* for the chosen PDF font size
        float pdfLineHeightPt = config.pdfFontSize * 1.2f; // Estimate line height (adjust multiplier as needed)
        // Get width of 'M' in points for average character width estimate
        // Note: PDF font size is already in points. HPDF_Font_TextWidth gives width in PDF units (1/1000 text point size).
        float charWidthPt = HPDF_Font_TextWidth(pdfFont, (const HPDF_BYTE *)"M", 1).width * config.pdfFontSize / 1000.0f;
        if (charWidthPt <= 0) charWidthPt = config.pdfFontSize * 0.6f; // Fallback estimate if width is 0

        // Add some margin
        float margin = 20.0f; // Margin in points
        HPDF_REAL page_width = static_cast<HPDF_REAL>(asciiData[0].size() * charWidthPt + 2 * margin);
        HPDF_REAL page_height = static_cast<HPDF_REAL>(asciiData.size() * pdfLineHeightPt + 2 * margin);

        HPDF_Page_SetWidth(page, page_width);
        HPDF_Page_SetHeight(page, page_height);

        // --- Draw Background Color ---
        HPDF_Page_SetRGBFill(page, bgColorUint[0] / 255.0f, bgColorUint[1] / 255.0f, bgColorUint[2] / 255.0f);
        HPDF_Page_Rectangle(page, 0, 0, page_width, page_height);
        HPDF_Page_Fill(page);

        // --- Prepare to Draw Text ---
        HPDF_Page_BeginText(page);
        HPDF_Page_SetFontAndSize(page, pdfFont, config.pdfFontSize);
        HPDF_Page_SetTextLeading(page, pdfLineHeightPt); // Set line spacing

        // PDF coordinate system: (0,0) is bottom-left.
        // We start drawing near the top-left.
        HPDF_REAL startX = margin;
        HPDF_REAL startY = page_height - margin - config.pdfFontSize; // Start near top margin (adjust if needed)

        HPDF_Page_MoveTextPos(page, startX, startY);


        // --- Draw ASCII Characters ---
        for (const auto& lineData : asciiData) {
            for (const auto& charInfo : lineData) {
                 // Set color for this character
                const unsigned char* finalColorUint = (scheme == ColorScheme::COLOR_ON_WHITE || scheme == ColorScheme::COLOR_ON_BLACK) ? charInfo.color : fgColorUint;
                 HPDF_Page_SetRGBFill(page, finalColorUint[0] / 255.0f, finalColorUint[1] / 255.0f, finalColorUint[2] / 255.0f);

                // Output the character (libHaru expects C-string)
                char charStr[2] = { charInfo.character, '\0' };
                HPDF_Page_ShowText(page, charStr);
                // Note: HPDF_ShowText advances the cursor automatically based on character width.
                // If precise positioning is needed (e.g., strict grid), use MoveTextPos repeatedly.
            }
            // Move to the beginning of the next line (down)
            // HPDF_Page_MoveTextPos advances relative to the *start* of the current line,
            // so we need to move to the start X and down by one line height (leading).
             HPDF_Page_MoveToNextLine(page); // Easier way to move down one line using leading

             // Manual way if MoveToNextLine isn't suitable:
             // currentY -= pdfLineHeightPt;
             // HPDF_Page_MoveTextPos(page, startX, currentY);
        }

        // --- Finalize Text and Save ---
        HPDF_Page_EndText(page);
        if (HPDF_SaveToFile(pdf, outputPath.string().c_str()) != HPDF_OK) {
           cerr << "PDF Error: Cannot save PDF to file: " << outputPath.string() << endl;
           throw runtime_error("Failed to save PDF file."); // Trigger cleanup
        }

    } catch (const std::exception& e) {
        cerr << "PDF Exception: " << e.what() << endl;
        HPDF_Free(pdf); // Cleanup PDF document object on error
        return false;
    } catch (...) {
        cerr << "PDF Error: Unknown exception during PDF generation." << endl;
        HPDF_Free(pdf); // Cleanup PDF document object on error
        return false;
    }

    // --- Cleanup ---
    HPDF_Free(pdf);
    return true;
}



// --- Modified: Process single image file (Calls saveImage and conditionally savePdf) ---
bool processImageFile(const path& imagePath,
                      const path& baseOutputDir, // Output sub-directory base
                      const Config& config,
                      const stbtt_fontinfo& fontInfo, // Pre-loaded font info
                      const vector<unsigned char>& fontBuffer) // Font data buffer
{
    cout << "\n==================================================" << endl;
    cout << "Processing image: " << imagePath.string() << endl;
    cout << "==================================================" << endl;

    // --- Setup output subdirectory ---
    // 创建包含 targetWidth 的新后缀
    string imageSubDirSuffixWithWidth = "_" + std::to_string(config.targetWidth) + config.imageOutputSubDirSuffix;
    // 使用新的后缀调用 setupImageOutputSubdirectory
    path outputSubDirPath = setupImageOutputSubdirectory(baseOutputDir, imagePath, imageSubDirSuffixWithWidth);
    
    if (outputSubDirPath.empty()) {
        cerr << "Skipping image due to output directory setup failure." << endl;
        return false;
    }

    // --- Load image ---
    cout << "Loading image..." << endl;
    auto load_start = high_resolution_clock::now();
    int width, height;
    auto imgDataPtr = loadImage(imagePath.string(), width, height);
    auto load_end = high_resolution_clock::now();
    auto load_duration = duration_cast<milliseconds>(load_end - load_start);
    cout << "-> Image loaded (" << width << "x" << height << "), Time: "
         << fixed << setprecision(3) << load_duration.count() / 1000.0 << " seconds" << endl;
    if (!imgDataPtr) {
        cerr << "Skipping image due to loading failure." << endl;
        return false;
    }

    // --- Start processing timer ---
    auto image_processing_start = high_resolution_clock::now();

    // --- Generate ASCII data ---
    cout << "Generating ASCII data..." << endl;
    auto ascii_conv_start = high_resolution_clock::now();
    // Calculate target height based on target width and aspect ratio correction
    int targetHeight = static_cast<int>(std::round(static_cast<double>(height * config.targetWidth) / (width * config.charAspectRatioCorrection))); // Use std::round
    targetHeight = max(1, targetHeight); // Ensure at least 1 row
    cout << "Calculated ASCII grid: " << config.targetWidth << "x" << targetHeight << endl;

    vector<vector<CharColorInfo>> asciiResultData = generateAsciiData(
        imgDataPtr.get(), width, height, config.targetWidth, targetHeight);
    auto ascii_conv_end = high_resolution_clock::now();
    if (asciiResultData.empty() || asciiResultData[0].empty()) { // Check if data or first row is empty
        cerr << "Error: Failed to generate valid ASCII data for this image. Skipping." << endl;
        return false;
    }
    auto ascii_conv_duration = duration_cast<milliseconds>(ascii_conv_end - ascii_conv_start);
    cout << "-> ASCII data conversion time: " << fixed << setprecision(3) << ascii_conv_duration.count() / 1000.0 << " seconds" << endl;


    // --- Calculate PNG Output Dimensions ---
    int charWidthPx, lineHeightPx, outputImageWidthPx, outputImageHeightPx, ascentPx, descentPx, lineGapPx;
    float scaleForPng;
    calculateOutputDimensions(fontInfo, config.fontSize, config.targetWidth, targetHeight,
                              charWidthPx, lineHeightPx, outputImageWidthPx, outputImageHeightPx,
                              scaleForPng, ascentPx, descentPx, lineGapPx);
    if (outputImageWidthPx <= 0 || outputImageHeightPx <= 0) {
        cerr << "Error: Calculated PNG output image dimensions are invalid (" << outputImageWidthPx << "x" << outputImageHeightPx << "). Skipping image." << endl;
        return false;
    }
     cout << "Calculated PNG output size: " << outputImageWidthPx << "x" << outputImageHeightPx << endl;


    // --- Process each selected scheme ---
    cout << "Processing color schemes for this image:" << endl;
    bool allSchemesSuccessful = true;
    for (const auto& currentScheme : config.schemesToGenerate) {
        auto theme_start = high_resolution_clock::now();

        string schemeSuffix = getSchemeSuffix(currentScheme);
        string baseNameForOutput = imagePath.stem().string() + schemeSuffix;

        // --- PNG Output ---
        string pngOutputFilename = baseNameForOutput + config.outputPngExtension;
        path finalPngOutputPath = outputSubDirPath / pngOutputFilename;
        cout << "  Processing scheme: " << schemeSuffix.substr(1)
             << ", Outputting PNG to: " << finalPngOutputPath.string() << endl;

        // a. Set colors
        unsigned char bgColor[3];
        unsigned char fgColor[3];
        setSchemeColors(currentScheme, bgColor, fgColor);

        // b. Create PNG buffer
        vector<unsigned char> outputImageData;
        try {
            size_t required_size = static_cast<size_t>(outputImageWidthPx) * outputImageHeightPx * OUTPUT_CHANNELS;
             // Add more robust size checks
             if (outputImageWidthPx <= 0 || outputImageHeightPx <= 0) {
                 throw std::runtime_error("Invalid PNG dimensions before allocation.");
             }
             if (static_cast<double>(outputImageWidthPx) * outputImageHeightPx * OUTPUT_CHANNELS > outputImageData.max_size()) {
                 throw std::runtime_error("Required PNG buffer size exceeds vector's maximum capacity.");
             }
             outputImageData.resize(required_size);
        } catch (const std::bad_alloc& e) {
            cerr << "Error: Failed to allocate PNG output buffer (Size: " << outputImageWidthPx << "x" << outputImageHeightPx << "). Out of memory? Skipping scheme." << endl;
            allSchemesSuccessful = false;
            continue; // Skip this scheme
        } catch (const std::exception& e) {
            cerr << "Error: Exception occurred while preparing PNG buffer: " << e.what() << ". Skipping scheme." << endl;
            allSchemesSuccessful = false;
            continue; // Skip this scheme
        }


        // c. Render PNG
        auto render_png_start = high_resolution_clock::now();
        renderAsciiArt(outputImageData, asciiResultData, fontInfo, currentScheme,
                       bgColor, fgColor, outputImageWidthPx, outputImageHeightPx,
                       ascentPx, scaleForPng, lineHeightPx);
        auto render_png_end = high_resolution_clock::now();
        auto render_png_duration = duration_cast<milliseconds>(render_png_end - render_png_start);
        cout << "    -> PNG Rendering time: " << fixed << setprecision(3) << render_png_duration.count() / 1000.0 << " seconds" << endl;


        // d. Save PNG
        auto save_png_start = high_resolution_clock::now();
        if (!saveImage(finalPngOutputPath, outputImageWidthPx, outputImageHeightPx, OUTPUT_CHANNELS, outputImageData)) {
            cerr << "  Error: Failed to save PNG for scheme " << schemeSuffix.substr(1) << "." << endl;
            allSchemesSuccessful = false; // Mark failure but continue processing other schemes/PDF
        } else {
            auto save_png_end = high_resolution_clock::now();
            auto save_png_duration = duration_cast<milliseconds>(save_png_end - save_png_start);
            cout << "    -> PNG Saving time: " << fixed << setprecision(3) << save_png_duration.count() / 1000.0 << " seconds" << endl;
            cout << "  Scheme PNG " << schemeSuffix.substr(1) << " saved successfully." << endl;
        }

        // --- PDF Output (Conditional based on config.generatePdfOutput) ---
        if (config.generatePdfOutput) { // <---- 在这里添加了检查
            string pdfOutputFilename = baseNameForOutput + config.outputPdfExtension;
            path finalPdfOutputPath = outputSubDirPath / pdfOutputFilename;
            cout << "  Outputting PDF to: " << finalPdfOutputPath.string() << endl;

            auto save_pdf_start = high_resolution_clock::now();
            if (!savePdf(finalPdfOutputPath, asciiResultData, config, currentScheme, bgColor, fgColor)) {
                 cerr << "  Error: Failed to save PDF for scheme " << schemeSuffix.substr(1) << "." << endl;
                 allSchemesSuccessful = false; // Mark failure
            } else {
                auto save_pdf_end = high_resolution_clock::now();
                auto save_pdf_duration = duration_cast<milliseconds>(save_pdf_end - save_pdf_start);
                 cout << "    -> PDF Saving time: " << fixed << setprecision(3) << save_pdf_duration.count() / 1000.0 << " seconds" << endl;
                 cout << "  Scheme PDF " << schemeSuffix.substr(1) << " saved successfully." << endl;
            }
        } else {
             cout << "  PDF output is disabled by internal configuration." << endl;
        }


        auto theme_end = high_resolution_clock::now();
        auto theme_duration = duration_cast<milliseconds>(theme_end - theme_start);
        // cout << "  -> Total processing time for this theme: " << fixed << setprecision(3) << theme_duration.count() / 1000.0 << " seconds" << endl;

    } // --- End scheme loop ---

    auto image_processing_end = high_resolution_clock::now();
    auto image_processing_duration = duration_cast<milliseconds>(image_processing_end - image_processing_start);
    cout << "-> Finished processing image '" << imagePath.filename().string() << "'. Total time: "
         << fixed << setprecision(3) << image_processing_duration.count() / 1000.0 << " seconds" << endl;

    return allSchemesSuccessful; // Return true only if ALL schemes (PNG and potentially PDF) succeeded for this image
}


// --- 主应用程序逻辑 ---
int main(int argc, char* argv[]) {
    Config config; // Initialize Config object with defaults
    const string configFilename = "config.txt";

    // --- Determine executable path and config path ---
    path exePath;
     try { // Get executable path more robustly
        if (argc > 0 && argv[0] != nullptr && filesystem::exists(argv[0])) {
            exePath = filesystem::canonical(argv[0]); // Get absolute path
         } else {
             // Fallback or error if argv[0] is unreliable
            #ifdef _WIN32 // Windows specific fallback
                 char pathBuf[MAX_PATH];
                  GetModuleFileNameA(NULL, pathBuf, MAX_PATH);
                  exePath = pathBuf;
            #else // Linux/macOS fallback (may need readlink /proc/self/exe or similar)
                 // For simplicity, using current_path as a less reliable fallback
                 cerr << "Warning: Could not reliably determine executable path from argv[0]. Using current directory." << endl;
                 exePath = filesystem::current_path() / "unknown_executable"; // Placeholder
            #endif
         }
    } catch (const filesystem::filesystem_error& e) {
         cerr << "Warning: Filesystem error resolving executable path: " << e.what() << endl;
         cerr << "Attempting to use current working directory as fallback." << endl;
         exePath = filesystem::current_path() / (argc > 0 && argv[0] ? filesystem::path(argv[0]).filename() : "unknown_executable");
    } catch (const std::exception& e) {
         cerr << "Warning: Error resolving executable path: " << e.what() << endl;
         exePath = filesystem::current_path() / "unknown_executable";
    }

    path exeDir = exePath.parent_path();
    path configPathObj = exeDir / configFilename;

    // --- Load Configuration ---
    loadConfiguration(configPathObj, config); // Pass Config object by reference
    // Construct final font path relative to executable directory
    config.finalFontPath = (exeDir / config.fontFilename).string();
    // Validate if the font file actually exists now
    if (!filesystem::exists(config.finalFontPath)) {
        cerr << "Error: Font file specified does not exist: " << config.finalFontPath << endl;
        cerr << "Please ensure '" << config.fontFilename << "' is in the same directory as the executable or provide a full path in config.txt." << endl;
        return 1;
    }


    // --- Print final configuration being used ---
    cout << "\n--- Using Configuration ---" << endl;
    cout << "Target Width (ASCII Chars): " << config.targetWidth << endl;
    cout << "Aspect Ratio Correction: " << config.charAspectRatioCorrection << endl;
    cout << "Font Filename: " << config.fontFilename << endl;
    cout << "Font Full Path: " << config.finalFontPath << endl;
    cout << "Font Size (PNG Pixels): " << config.fontSize << endl;
    cout << "Font Size (PDF Points): " << config.pdfFontSize << endl; // Print PDF font size
    // 打印 PDF 输出设置
    cout << "Generate PDF Output: " << (config.generatePdfOutput ? "Enabled" : "Disabled") << " (Internal Setting)" << endl;
    cout << "--------------------------" << endl;

    // --- Load Font (once) ---
    stbtt_fontinfo fontInfo;
    vector<unsigned char> fontBuffer; // Must stay in scope while fontInfo is used
    if (!loadFontInfo(config.finalFontPath, fontInfo, fontBuffer)) {
        // Error message already printed by loadFontInfo or earlier check
        return 1;
    }

    // --- Get Input Path ---
    string inputPathStr = getUserInputPath();
    if (inputPathStr.empty()) return 1;
    path inputPath(inputPathStr);

    int processedCount = 0;
    int failedCount = 0;
    auto overall_start_time = high_resolution_clock::now();

    // --- Check input path type ---
    try {
        if (!filesystem::exists(inputPath)) {
            cerr << "Error: Input path does not exist: " << inputPath.string() << endl;
            return 1;
        }

        if (filesystem::is_regular_file(inputPath)) {
            // --- Process Single File ---
            cout << "\nInput is a single file. Processing..." << endl;
            if (isImageFile(inputPath)) {
                path baseOutputDir = inputPath.parent_path(); // Output to image's directory
                // Pass fontBuffer to processImageFile (although PDF uses path now)
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
            // --- Process Directory ---
            cout << "\nInput is a directory. Processing image files inside..." << endl;
            
           // 构建包含 targetWidth 的批处理目录名
           string batchDirName = inputPath.filename().string() + "_" + std::to_string(config.targetWidth) + config.batchOutputSubDirSuffix;
           // 使用新的目录名创建路径
           path batchBaseOutputDir = inputPath.parent_path() / batchDirName;
            try {
                 if(filesystem::create_directories(batchBaseOutputDir)) {
                     cout << "Created main batch output directory: " << batchBaseOutputDir.string() << endl;
                 } else if (!filesystem::is_directory(batchBaseOutputDir)) {
                     cerr << "Error: Failed to create or access main batch output directory: " << batchBaseOutputDir.string() << endl;
                     return 1; // Exit if cannot create main output dir
                 }
            } catch (const filesystem::filesystem_error& e) {
                 cerr << "Error creating main batch output directory: " << batchBaseOutputDir.string() << " - " << e.what() << endl;
                 return 1;
            }


            filesystem::directory_iterator dir_iter(inputPath);
            filesystem::directory_iterator end_iter; // Default constructed end iterator

            vector<path> imageFilesToProcess;
            for (; dir_iter != end_iter; ++dir_iter) {
                const auto& entry = *dir_iter;
                if (entry.is_regular_file() && isImageFile(entry.path())) {
                    imageFilesToProcess.push_back(entry.path());
                }
            }

            if (imageFilesToProcess.empty()) {
                 cout << "No supported image files found in the directory: " << inputPath.string() << endl;
            } else {
                cout << "Found " << imageFilesToProcess.size() << " image(s) to process." << endl;
                // --- TODO: Implement parallel processing here if desired ---
                // Example: Use std::async or a thread pool to call processImageFile
                // for each path in imageFilesToProcess. Remember thread safety if
                // modifying shared counters directly.
                for(const auto& imgPath : imageFilesToProcess) {
                     if (processImageFile(imgPath, batchBaseOutputDir, config, fontInfo, fontBuffer)) {
                         processedCount++;
                     } else {
                         failedCount++;
                     }
                }
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
    duration<double> overall_duration_seconds = overall_end_time - overall_start_time;

    cout << "\n==================================================" << endl;
    cout << "Overall Processing Summary:" << endl;
    cout << "  Successfully processed: " << processedCount << " image(s)" << endl;
    cout << "  Failed/Skipped:       " << failedCount << " image(s)" << endl;
    cout << "  Total time elapsed:   " << fixed << setprecision(3)
         << overall_duration_seconds.count() << " seconds" << endl;
    cout << "==================================================" << endl;

    // Keep console open
    cout << "\nPress Enter to exit..." << endl;
    // 使用 cin.ignore() 清除可能存在的残留换行符
    // cin.ignore(numeric_limits<streamsize>::max(), '\n');
    // 然后等待用户按 Enter
    cin.get();


    return (failedCount > 0); // Return non-zero if any failures occurred
}
