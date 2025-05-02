#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream> // For reading font file
#include <memory>  // For unique_ptr
#include <filesystem> // For path manipulation and directory creation (needs C++17)
#include <system_error> // For filesystem error handling

// Define this macro so stb_image.h includes the implementation
// Make sure this definition only appears in one .cpp file
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // Assume stb_image.h is in the same directory or include path

// Define implementation for stb_image_write
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h" // Assume stb_image_write.h is here

// Define implementation for stb_truetype
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h" // Assume stb_truetype.h is here

// --- Bring std namespace into scope ---
using namespace std;
// --- Bring specific nested namespace members into scope ---
using std::filesystem::path; // Now we can use 'path' directly

// --- 颜色方案枚举 (保持不变) ---
enum class ColorScheme {
    BLACK_ON_WHITE,    // 黑字白底 (固定颜色)
    WHITE_ON_BLACK,    // 白字黑底 (固定颜色)
    GREEN_ON_BLACK,    // 绿字黑底 (固定颜色)
    PURPLE_ON_BLACK,   // 紫字黑底 (固定颜色)
    COLOR_ON_WHITE,    // 彩色字白底 (颜色来自原图)
    COLOR_ON_BLACK     // 彩色字黑底 (颜色来自原图)
};

// --- 结构体用于存储字符及其颜色 (保持不变) ---
struct CharColorInfo {
    char character;
    unsigned char color[3]; // R, G, B from original image
};

// Helper function to read a file into a vector of unsigned char (保持不变)
vector<unsigned char> read_file(const string& filename) {
    ifstream file(filename, ios::binary | ios::ate);
    if (!file) {
        cerr << "错误: 无法打开文件 '" << filename << "'" << endl;
        return {};
    }
    streamsize size = file.tellg();
    file.seekg(0, ios::beg);
    vector<unsigned char> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
         cerr << "错误: 无法读取文件 '" << filename << "'" << endl;
        return {};
    }
    return buffer;
}

// --- 新增：辅助函数，根据颜色方案获取文件名后缀 ---
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


int main() {
    string imagePath;
    const int targetWidth = 128;
    const double charAspectRatioCorrection = 2.0;
    const string fontPath = "C:\\Computer\\Code666\\github_cpp\\pics\\Consolas.ttf";
    const float fontSize = 15.0f;
    const string baseOutputFilename = "output_ascii_art"; // 基础名，不含后缀
    const string outputExtension = ".png";             // 后缀
    const string outputSubDirName = "ascii_output";

    // --- 修改：在此处选择要生成的颜色方案列表 ---
    const vector<ColorScheme> schemesToGenerate = {
        ColorScheme::BLACK_ON_WHITE,
        ColorScheme::WHITE_ON_BLACK,
        ColorScheme::GREEN_ON_BLACK,
        ColorScheme::COLOR_ON_WHITE,
        //ColorScheme::COLOR_ON_BLACK
        // 在这里添加或删除你想要的方案
    };
    // --- 颜色方案列表选择结束 ---

    cout << "请输入图片文件路径: ";
    getline(cin, imagePath);

    // --- 构造基础输出路径并创建子目录 (只执行一次) ---
    path inputPathObj(imagePath);
    path parentDir = inputPathObj.parent_path();
    path outputSubDirPath = parentDir / outputSubDirName;

    try {
        if (filesystem::create_directory(outputSubDirPath)) {
            cout << "已创建输出子目录: " << outputSubDirPath.string() << endl;
        } else {
             if (!filesystem::exists(outputSubDirPath)) {
                 cerr << "错误: 无法创建输出子目录: " << outputSubDirPath.string() << endl;
                 return 1;
             }
        }
    } catch (const filesystem::filesystem_error& e) {
        cerr << "错误: 创建输出子目录时发生异常: " << outputSubDirPath.string() << endl;
        cerr << "错误详情: " << e.what() << endl;
         if (!filesystem::is_directory(outputSubDirPath)) return 1;
         cerr << "警告: 尝试继续，假设目录已存在..." << endl;
    } catch (const std::exception& e) {
        cerr << "错误: 创建目录时发生未知异常: " << e.what() << endl;
        return 1;
    }
    // --- 路径构造和目录创建结束 ---

    if (targetWidth <= 0) {
        cerr << "错误: 配置的 targetWidth (" << targetWidth << ") 必须是正整数。" << endl;
        return 1;
    }

    // --- 加载输入图片 (只执行一次) ---
    int width, height, channels;
    unsigned char *imgData = stbi_load(imagePath.c_str(), &width, &height, &channels, 3); // 请求 3 通道
    if (imgData == nullptr) {
        cerr << "错误: 无法加载图片 '" << imagePath << "' (或无法转换为RGB)" << endl;
        cerr << "STB Image 错误原因: " << stbi_failure_reason() << endl;
        return 1;
    }
    channels = 3; // 确认通道数为3
    unique_ptr<unsigned char, void(*)(void*)> imgDataPtr(imgData, stbi_image_free);
    cout << "图片加载成功 (强制RGB): " << width << "x" << height << ", 通道数: " << channels << endl;
    cout << "目标ASCII宽度 (来自变量): " << targetWidth << endl;
    // --- 图片加载结束 ---

    // --- 计算和打印目标高度 (只执行一次) ---
    int targetHeight = static_cast<int>(round(static_cast<double>(height * targetWidth) / (width * charAspectRatioCorrection)));
    targetHeight = max(1, targetHeight);
    cout << "计算出的ASCII高度: " << targetHeight << endl;
    // --- 高度计算结束 ---

    // --- 定义 ASCII 字符集和计算缩放比例 (只执行一次) ---
    const string asciiChars = "@%#*+=-:. ";
    const int numAsciiChars = static_cast<int>(asciiChars.length());
    double xScale = static_cast<double>(width) / targetWidth;
    double yScale = static_cast<double>(height) / targetHeight;
    // --- ASCII 设置结束 ---

    // --- 生成 ASCII 字符和颜色数据 (只执行一次) ---
    cout << "\n正在生成ASCII数据..." << endl;
    vector<vector<CharColorInfo>> asciiResultData;
    asciiResultData.reserve(targetHeight);
    for (int yOut = 0; yOut < targetHeight; ++yOut) {
        vector<CharColorInfo> currentLineData;
        currentLineData.reserve(targetWidth);
        for (int xOut = 0; xOut < targetWidth; ++xOut) {
            int xImg = static_cast<int>(floor((xOut + 0.5) * xScale));
            int yImg = static_cast<int>(floor((yOut + 0.5) * yScale));
            xImg = max(0, min(xImg, width - 1));
            yImg = max(0, min(yImg, height - 1));
            size_t pixelOffset = (static_cast<size_t>(yImg) * width + xImg) * 3;
            unsigned char r = imgData[pixelOffset];
            unsigned char g = imgData[pixelOffset + 1];
            unsigned char b = imgData[pixelOffset + 2];
            int gray = (static_cast<int>(r) + g + b) / 3;
            int asciiIndex = static_cast<int>(floor((gray / 255.0f) * (numAsciiChars - 1)));
            asciiIndex = max(0, min(asciiIndex, numAsciiChars - 1));
            CharColorInfo info;
            info.character = asciiChars[static_cast<size_t>(asciiIndex)];
            info.color[0] = r; info.color[1] = g; info.color[2] = b;
            currentLineData.push_back(info);
        }
        asciiResultData.push_back(currentLineData);
    }
    cout << "ASCII数据生成完毕。" << endl;
    // --- ASCII 数据生成结束 ---

    // --- 加载字体文件和计算基本度量 (只执行一次) ---
    cout << "\n正在加载字体..." << endl;
    vector<unsigned char> fontBuffer = read_file(fontPath);
    if (fontBuffer.empty()) return 1;
    stbtt_fontinfo fontInfo;
    if (!stbtt_InitFont(&fontInfo, fontBuffer.data(), stbtt_GetFontOffsetForIndex(fontBuffer.data(), 0))) {
        cerr << "错误: 初始化字体失败。" << endl; return 1;
    }
    float scale = stbtt_ScaleForPixelHeight(&fontInfo, fontSize);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
    ascent = static_cast<int>(round(ascent * scale));
    descent = static_cast<int>(round(descent * scale));
    lineGap = static_cast<int>(round(lineGap * scale));
    int lineHeight = ascent - descent + lineGap;
    int advanceWidth, leftSideBearing;
    stbtt_GetCodepointHMetrics(&fontInfo, 'M', &advanceWidth, &leftSideBearing); // 用 'M' 估算
    int charWidth = static_cast<int>(round(advanceWidth * scale));
    int outputImageWidth = targetWidth * charWidth;
    int outputImageHeight = targetHeight * lineHeight;
    const int outputChannels = 3;
    cout << "字体加载和度量计算完毕。" << endl;
    // --- 字体加载结束 ---


    // --- 新增：循环处理每种选定的颜色方案 ---
    cout << "\n开始处理选定的颜色方案:" << endl;
    for (const auto& currentScheme : schemesToGenerate) {

        string schemeSuffix = getSchemeSuffix(currentScheme);
        string currentOutputFilename = baseOutputFilename + schemeSuffix + outputExtension;
        path finalOutputPathObj = outputSubDirPath / currentOutputFilename;
        string finalOutputPathString = finalOutputPathObj.string();

        cout << "  正在处理方案: " << schemeSuffix.substr(1) // 去掉前导下划线
             << ", 输出到: " << finalOutputPathString << endl;

        // --- 以下操作在循环内，针对当前方案进行 ---

        unsigned char bgColor[3];
        unsigned char fgColor[3]; // 对固定颜色方案仍有用

        // 根据当前方案设置颜色
        switch (currentScheme) {
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
                fgColor[0] = 0;   fgColor[1] = 0;   fgColor[2] = 0; // 默认
                break;
            case ColorScheme::COLOR_ON_BLACK:
                bgColor[0] = 0;   bgColor[1] = 0;   bgColor[2] = 0;
                fgColor[0] = 255; fgColor[1] = 255; fgColor[2] = 255; // 默认
                break;
            case ColorScheme::BLACK_ON_WHITE:
            default:
                bgColor[0] = 255; bgColor[1] = 255; bgColor[2] = 255;
                fgColor[0] = 0;   fgColor[1] = 0;   fgColor[2] = 0;
                break;
        }

        // 创建当前方案的图像缓冲区
        vector<unsigned char> outputImageData(static_cast<size_t>(outputImageWidth) * outputImageHeight * outputChannels);

        // 填充背景
        for (size_t i = 0; i < outputImageData.size(); i += outputChannels) {
            outputImageData[i]     = bgColor[0];
            outputImageData[i + 1] = bgColor[1];
            outputImageData[i + 2] = bgColor[2];
        }

        // 渲染字符到当前缓冲区
        int currentY = ascent;
        for (const auto& lineData : asciiResultData) {
            int currentX = 0;
            for (const auto& charInfo : lineData) {
                char c = charInfo.character;
                int char_w, char_h, xoff, yoff;
                unsigned char* bitmap = stbtt_GetCodepointBitmap(&fontInfo, scale, scale, c, &char_w, &char_h, &xoff, &yoff);
                 if (bitmap) {
                     stbtt_GetCodepointHMetrics(&fontInfo, c, &advanceWidth, &leftSideBearing);
                     int drawX_base = currentX + static_cast<int>(round(leftSideBearing * scale)) + xoff;
                     int drawY_base = currentY + yoff;
                     for (int y = 0; y < char_h; ++y) {
                         for (int x = 0; x < char_w; ++x) {
                             int outX = drawX_base + x;
                             int outY = drawY_base + y;
                             if (outX >= 0 && outX < outputImageWidth && outY >= 0 && outY < outputImageHeight) {
                                 unsigned char alpha = bitmap[y * char_w + x];
                                 if (alpha > 128) {
                                     size_t pixelIndex = (static_cast<size_t>(outY) * outputImageWidth + outX) * outputChannels;
                                     // --- 颜色选择逻辑，使用当前循环的 currentScheme ---
                                     if (currentScheme == ColorScheme::COLOR_ON_WHITE || currentScheme == ColorScheme::COLOR_ON_BLACK) {
                                         outputImageData[pixelIndex]     = charInfo.color[0];
                                         outputImageData[pixelIndex + 1] = charInfo.color[1];
                                         outputImageData[pixelIndex + 2] = charInfo.color[2];
                                     } else {
                                         outputImageData[pixelIndex]     = fgColor[0];
                                         outputImageData[pixelIndex + 1] = fgColor[1];
                                         outputImageData[pixelIndex + 2] = fgColor[2];
                                     }
                                 }
                             }
                         }
                     }
                     stbtt_FreeBitmap(bitmap, nullptr);
                     currentX += static_cast<int>(round(advanceWidth * scale));
                } else {
                     stbtt_GetCodepointHMetrics(&fontInfo, ' ', &advanceWidth, &leftSideBearing);
                     currentX += static_cast<int>(round(advanceWidth * scale));
                }
            }
            currentY += lineHeight;
        } // --- 渲染结束 ---

        // 保存当前方案的图像
        if (!stbi_write_png(finalOutputPathString.c_str(), outputImageWidth, outputImageHeight, outputChannels, outputImageData.data(), outputImageWidth * outputChannels)) {
            cerr << "  错误: 无法将ASCII艺术保存为图片 '" << finalOutputPathString << "'" << endl;
            // 选择继续处理下一个方案，或者在这里 return 1;
        } else {
            cout << "  方案 " << schemeSuffix.substr(1) << " 已成功保存。" << endl;
        }

        // outputImageData 在 vector 离开作用域时自动释放内存

    } // --- 颜色方案循环结束 ---

    cout << "\n所有选定的颜色方案处理完毕。" << endl;

    return 0;
}
