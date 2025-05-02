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

// --- 修改：扩展颜色方案枚举 ---
enum class ColorScheme {
    BLACK_ON_WHITE,    // 黑字白底 (固定颜色)
    WHITE_ON_BLACK,    // 白字黑底 (固定颜色)
    GREEN_ON_BLACK,    // 绿字黑底 (固定颜色)
    PURPLE_ON_BLACK,   // 紫字黑底 (固定颜色)
    COLOR_ON_WHITE,    // 彩色字白底 (颜色来自原图)
    COLOR_ON_BLACK     // 彩色字黑底 (颜色来自原图)
};

// --- 新增：结构体用于存储字符及其颜色 ---
struct CharColorInfo {
    char character;
    unsigned char color[3]; // R, G, B from original image
};

// Helper function to read a file into a vector of unsigned char
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


int main() {
    string imagePath;
    const int targetWidth = 128; // 调小宽度以便观察彩色效果
    const double charAspectRatioCorrection = 2.0;
    const string fontPath = "C:\\Computer\\Code666\\github_cpp\\pics\\Consolas.ttf"; // 确保字体路径正确
    const float fontSize = 15.0f;
    const string baseOutputFilename = "output_ascii_art_color.png"; // 修改文件名以区分
    const string outputSubDirName = "ascii_output";

    // --- 在此处选择颜色方案 ---
    // 通过修改下面这行来选择颜色搭配
    const ColorScheme currentScheme = ColorScheme::COLOR_ON_BLACK; // <--- 选择彩色模式
    // 可选项:
    // ColorScheme::BLACK_ON_WHITE
    // ColorScheme::WHITE_ON_BLACK
    // ColorScheme::GREEN_ON_BLACK
    // ColorScheme::PURPLE_ON_BLACK
    // ColorScheme::COLOR_ON_WHITE // <--- 新增
    // ColorScheme::COLOR_ON_BLACK // <--- 新增
    // --- 颜色方案选择结束 ---

    unsigned char bgColor[3];
    unsigned char fgColor[3]; // fgColor 现在只用于固定颜色模式

    // --- 根据选择的方案设置颜色值 ---
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
        // --- 新增彩色模式的背景设置 ---
        case ColorScheme::COLOR_ON_WHITE:
            bgColor[0] = 255; bgColor[1] = 255; bgColor[2] = 255; // White background
            // fgColor 在此模式下不重要，但可以给个默认值
            fgColor[0] = 0;   fgColor[1] = 0;   fgColor[2] = 0;
            break;
        case ColorScheme::COLOR_ON_BLACK:
            bgColor[0] = 0;   bgColor[1] = 0;   bgColor[2] = 0;   // Black background
             // fgColor 在此模式下不重要
            fgColor[0] = 255;   fgColor[1] = 255;   fgColor[2] = 255;
            break;
        // --- 彩色模式结束 ---
        case ColorScheme::BLACK_ON_WHITE:
        default:
            bgColor[0] = 255; bgColor[1] = 255; bgColor[2] = 255;
            fgColor[0] = 0;   fgColor[1] = 0;   fgColor[2] = 0;
            break;
    }
    // --- 颜色设置结束 ---

    cout << "请输入图片文件路径: ";
    getline(cin, imagePath);

    // --- 构造输出路径并创建子目录 ---
    path inputPathObj(imagePath);
    path parentDir = inputPathObj.parent_path();
    path outputSubDirPath = parentDir / outputSubDirName;

    try {
        if (filesystem::create_directory(outputSubDirPath)) {
            cout << "已创建输出子目录: " << outputSubDirPath.string() << endl;
        } else {
             if (!filesystem::exists(outputSubDirPath)) {
                 cerr << "错误: 无法创建输出子目录，但目录似乎也不存在: " << outputSubDirPath.string() << endl;
                 return 1;
             }
        }
    } catch (const filesystem::filesystem_error& e) {
        cerr << "错误: 创建输出子目录时发生异常: " << outputSubDirPath.string() << endl;
        cerr << "错误详情: " << e.what() << endl;
         if (!filesystem::is_directory(outputSubDirPath)) {
              return 1;
         }
         cerr << "警告: 尝试继续，假设目录已存在..." << endl;
    } catch (const std::exception& e) {
        cerr << "错误: 创建目录时发生未知异常: " << e.what() << endl;
        return 1;
    }

    path finalOutputPathObj = outputSubDirPath / baseOutputFilename;
    string finalOutputPathString = finalOutputPathObj.string();
    cout << "输出文件将保存到: " << finalOutputPathString << endl;
    // --- 路径构造和目录创建结束 ---

    if (targetWidth <= 0) {
        cerr << "错误: 配置的 targetWidth (" << targetWidth << ") 必须是正整数。" << endl;
        return 1;
    }

    int width, height, channels;
    // --- 修改：强制加载至少3通道 (RGB) 以获取颜色信息 ---
    // 如果原始图像没有3通道，stb_image 会尝试转换，但这可能不是最佳颜色表示
    // 如果原始图像是灰度图，彩色模式可能效果不佳
    unsigned char *imgData = stbi_load(imagePath.c_str(), &width, &height, &channels, 3); // 请求 3 通道

    if (imgData == nullptr) {
        cerr << "错误: 无法加载图片 '" << imagePath << "' (或无法转换为RGB)" << endl;
        cerr << "STB Image 错误原因: " << stbi_failure_reason() << endl;
        return 1;
    }
    // 更新实际加载的通道数（现在应该是 3 或加载失败）
    channels = 3;
    unique_ptr<unsigned char, void(*)(void*)> imgDataPtr(imgData, stbi_image_free);

    cout << "图片加载成功 (强制RGB): " << width << "x" << height << ", 通道数: " << channels << endl;
    cout << "目标ASCII宽度 (来自变量): " << targetWidth << endl;

    int targetHeight = static_cast<int>(round(static_cast<double>(height * targetWidth) / (width * charAspectRatioCorrection)));
    targetHeight = max(1, targetHeight);
    cout << "计算出的ASCII高度: " << targetHeight << endl;

    const string asciiChars = "@%#*+=-:. ";
    const int numAsciiChars = static_cast<int>(asciiChars.length());

    double xScale = static_cast<double>(width) / targetWidth;
    double yScale = static_cast<double>(height) / targetHeight;

    // --- 修改：使用新的数据结构存储字符和颜色 ---
    vector<vector<CharColorInfo>> asciiResultData;
    asciiResultData.reserve(targetHeight);

    // --- 修改：ASCII 生成循环，存储字符和颜色 ---
    for (int yOut = 0; yOut < targetHeight; ++yOut) {
        vector<CharColorInfo> currentLineData; // 存储当前行的字符和颜色信息
        currentLineData.reserve(targetWidth);
        for (int xOut = 0; xOut < targetWidth; ++xOut) {
            int xImg = static_cast<int>(floor((xOut + 0.5) * xScale));
            int yImg = static_cast<int>(floor((yOut + 0.5) * yScale));
            xImg = max(0, min(xImg, width - 1));
            yImg = max(0, min(yImg, height - 1));

            // 确保使用 3 通道计算偏移
            size_t pixelOffset = (static_cast<size_t>(yImg) * width + xImg) * 3;

            // 获取 R, G, B (现在保证有3通道)
            unsigned char r = imgData[pixelOffset];
            unsigned char g = imgData[pixelOffset + 1];
            unsigned char b = imgData[pixelOffset + 2];

            // 计算灰度值以选择字符
            int gray = (static_cast<int>(r) + g + b) / 3;
            int asciiIndex = static_cast<int>(floor((gray / 255.0f) * (numAsciiChars - 1)));
            asciiIndex = max(0, min(asciiIndex, numAsciiChars - 1));

            // 创建 CharColorInfo 对象并填充
            CharColorInfo info;
            info.character = asciiChars[static_cast<size_t>(asciiIndex)];
            info.color[0] = r;
            info.color[1] = g;
            info.color[2] = b;

            // 添加到当前行数据
            currentLineData.push_back(info);
        }
        // 将当前行数据添加到结果中
        asciiResultData.push_back(currentLineData);
    }
    // --- ASCII 生成结束 ---

    cout << "\n正在渲染ASCII艺术为图片..." << endl;

    vector<unsigned char> fontBuffer = read_file(fontPath);
    if (fontBuffer.empty()) {
        return 1;
    }

    stbtt_fontinfo fontInfo;
    if (!stbtt_InitFont(&fontInfo, fontBuffer.data(), stbtt_GetFontOffsetForIndex(fontBuffer.data(), 0))) {
        cerr << "错误: 初始化字体失败。" << endl;
        return 1;
    }

    float scale = stbtt_ScaleForPixelHeight(&fontInfo, fontSize);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
    ascent = static_cast<int>(round(ascent * scale));
    descent = static_cast<int>(round(descent * scale));
    lineGap = static_cast<int>(round(lineGap * scale));
    int lineHeight = ascent - descent + lineGap;

    int advanceWidth, leftSideBearing;
    stbtt_GetCodepointHMetrics(&fontInfo, 'M', &advanceWidth, &leftSideBearing);
    int charWidth = static_cast<int>(round(advanceWidth * scale));

    int outputImageWidth = targetWidth * charWidth;
    int outputImageHeight = targetHeight * lineHeight;
    const int outputChannels = 3;

    vector<unsigned char> outputImageData(static_cast<size_t>(outputImageWidth) * outputImageHeight * outputChannels);
    // 背景填充
    for (size_t i = 0; i < outputImageData.size(); i += outputChannels) {
        outputImageData[i]     = bgColor[0];
        outputImageData[i + 1] = bgColor[1];
        outputImageData[i + 2] = bgColor[2];
    }

    int currentY = ascent;
    // --- 修改：渲染循环，使用 CharColorInfo ---
    for (const auto& lineData : asciiResultData) { // 遍历行数据
        int currentX = 0;
        for (const auto& charInfo : lineData) { // 遍历行中的 CharColorInfo
            char c = charInfo.character; // 获取字符
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
                             if (alpha > 128) { // 如果字体像素不透明
                                 size_t pixelIndex = (static_cast<size_t>(outY) * outputImageWidth + outX) * outputChannels;

                                 // --- 修改：根据模式选择颜色 ---
                                 if (currentScheme == ColorScheme::COLOR_ON_WHITE || currentScheme == ColorScheme::COLOR_ON_BLACK) {
                                     // 彩色模式：使用存储的原始颜色
                                     outputImageData[pixelIndex]     = charInfo.color[0]; // R
                                     outputImageData[pixelIndex + 1] = charInfo.color[1]; // G
                                     outputImageData[pixelIndex + 2] = charInfo.color[2]; // B
                                 } else {
                                     // 固定颜色模式：使用全局 fgColor
                                     outputImageData[pixelIndex]     = fgColor[0];
                                     outputImageData[pixelIndex + 1] = fgColor[1];
                                     outputImageData[pixelIndex + 2] = fgColor[2];
                                 }
                                 // --- 颜色选择结束 ---
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
    }
    // --- 渲染循环结束 ---

    if (!stbi_write_png(finalOutputPathString.c_str(), outputImageWidth, outputImageHeight, outputChannels, outputImageData.data(), outputImageWidth * outputChannels)) {
        cerr << "错误: 无法将ASCII艺术保存为图片 '" << finalOutputPathString << "'" << endl;
        return 1;
    }

    cout << "ASCII艺术已成功渲染并保存到: " << finalOutputPathString << endl;

    return 0;
}
