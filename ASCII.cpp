#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream> // For reading font file
#include <memory>  // For unique_ptr

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

using namespace std;

// Helper function to read a file into a vector of unsigned char
std::vector<unsigned char> read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        cerr << "错误: 无法打开文件 '" << filename << "'" << endl;
        return {};
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
         cerr << "错误: 无法读取文件 '" << filename << "'" << endl;
        return {};
    }
    return buffer;
}


int main() {
    string imagePath;
    const int targetWidth = 120; // 例如，设置为 80 个字符宽
    const double charAspectRatioCorrection = 2.0; // 字符高宽比修正
    const string fontPath = "C:\\Computer\\Code666\\github_cpp\\pics\\Consolas.ttf"; //需要一个字体文件！推荐等宽字体
    const float fontSize = 15.0f;       // <--- 字体大小 (像素)
    const string outputImagePath = "output_ascii_art.png"; // 输出图片文件名
    const unsigned char bgColor[3] = {255, 255, 255}; // 背景色 (白色)
    const unsigned char fgColor[3] = {0, 0, 0};       // 前景色/文字颜色 (黑色)


    cout << "请输入图片文件路径: ";
    getline(cin, imagePath); // Use getline for paths with spaces

    // 验证硬编码的宽度是否有效
    if (targetWidth <= 0) {
        cerr << "错误: 配置的 targetWidth (" << targetWidth << ") 必须是正整数。" << endl;
        return 1;
    }

    int width, height, channels;
    // Load the image file
    unsigned char *imgData = stbi_load(imagePath.c_str(), &width, &height, &channels, 0);

    // Check if image loading was successful
    if (imgData == nullptr) {
        cerr << "错误: 无法加载图片 '" << imagePath << "'" << endl;
        cerr << "STB Image 错误原因: " << stbi_failure_reason() << endl;
        return 1; // Return error code
    }
    // 使用 unique_ptr 确保 imgData 被释放
    std::unique_ptr<unsigned char, void(*)(void*)> imgDataPtr(imgData, stbi_image_free);


    cout << "图片加载成功: " << width << "x" << height << ", 通道数: " << channels << endl;
    cout << "目标ASCII宽度 (来自变量): " << targetWidth << endl; // 明确宽度来源


    // Calculate target height based on target width and aspect ratio correction
    int targetHeight = static_cast<int>(round(static_cast<double>(height * targetWidth) / (width * charAspectRatioCorrection)));
    targetHeight = max(1, targetHeight); // Ensure height is at least 1

    cout << "计算出的ASCII高度: " << targetHeight << endl;


    // Define ASCII characters for grayscale levels (dark to light)
    const string asciiChars = "@%#*+=-:. "; // Common ramp (dense chars for dark areas)
    const int numAsciiChars = asciiChars.length();

    // Calculate scaling factors: how many original pixels map to one output character
    double xScale = static_cast<double>(width) / targetWidth;
    double yScale = static_cast<double>(height) / targetHeight;

    // --- 新增: 存储ASCII结果 ---
    vector<string> asciiResultLines;
    asciiResultLines.reserve(targetHeight);

    // Iterate through the rows and columns of the target ASCII representation
    for (int yOut = 0; yOut < targetHeight; ++yOut) {
        string currentLine = "";
        currentLine.reserve(targetWidth);
        for (int xOut = 0; xOut < targetWidth; ++xOut) {

            // Find the corresponding pixel coordinates in the original image
            int xImg = static_cast<int>(floor((xOut + 0.5) * xScale));
            int yImg = static_cast<int>(floor((yOut + 0.5) * yScale));

            // Clamp coordinates to be within image bounds (safety check)
            xImg = max(0, min(xImg, width - 1));
            yImg = max(0, min(yImg, height - 1));

            // Calculate the index for the sampled pixel in the image data array
            unsigned long long pixelOffset = (static_cast<unsigned long long>(yImg) * width + xImg) * channels;

            // Get the R, G, B values of the sampled pixel
            unsigned char r = imgData[pixelOffset];
            unsigned char g = (channels > 1) ? imgData[pixelOffset + 1] : r; // Handle grayscale
            unsigned char b = (channels > 2) ? imgData[pixelOffset + 2] : r; // Handle grayscale/two-channel

            // Calculate grayscale intensity (simple average)
            int gray = (r + g + b) / 3;

            // Map grayscale value (0-255) to an ASCII character index
            int asciiIndex = static_cast<int>(floor((gray / 255.0f) * (numAsciiChars - 1)));
            asciiIndex = max(0, min(asciiIndex, numAsciiChars - 1)); // Clamp index

            // --- 修改: 不再打印，而是存储 ---
            // cout << asciiChars[asciiIndex];
            currentLine += asciiChars[asciiIndex];
        }
        // --- 修改: 存储行并打印换行 (可选，用于控制台预览) ---
         asciiResultLines.push_back(currentLine);
         // cout << currentLine << endl; // 可以取消注释这行来看控制台输出
    }

    // --- 新增: 将ASCII结果渲染为图片 ---
    cout << "\n正在渲染ASCII艺术为图片..." << endl;

    // 1. 加载字体文件
    vector<unsigned char> fontBuffer = read_file(fontPath);
    if (fontBuffer.empty()) {
        cerr << "错误: 无法加载或读取字体文件 '" << fontPath << "'" << endl;
        // imgDataPtr 会在 main 结束时自动释放 imgData
        return 1;
    }

    stbtt_fontinfo fontInfo;
    if (!stbtt_InitFont(&fontInfo, fontBuffer.data(), stbtt_GetFontOffsetForIndex(fontBuffer.data(), 0))) {
        cerr << "错误: 初始化字体失败。" << endl;
        return 1;
    }

    // 2. 计算字体度量和图片尺寸
    float scale = stbtt_ScaleForPixelHeight(&fontInfo, fontSize);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
    ascent = static_cast<int>(round(ascent * scale));
    descent = static_cast<int>(round(descent * scale));
    lineGap = static_cast<int>(round(lineGap * scale));
    int lineHeight = ascent - descent + lineGap;

    // 计算单个字符的大致宽度 (使用一个常见字符如 'M' 或 平均值)
    // 更精确的方法是计算每一行所有字符的总宽度，然后取最大值
    int advanceWidth, leftSideBearing;
    stbtt_GetCodepointHMetrics(&fontInfo, 'M', &advanceWidth, &leftSideBearing); // 用'M'估算
    int charWidth = static_cast<int>(round(advanceWidth * scale));
    // 注意：对于非等宽字体，这里可能不够准确，图片宽度可能需要动态调整或设为最大行宽

    int outputImageWidth = targetWidth * charWidth; // 估算的宽度
    int outputImageHeight = targetHeight * lineHeight;
    const int outputChannels = 3; // RGB

    // 3. 创建图像缓冲区并填充背景色
     vector<unsigned char> outputImageData(static_cast<size_t>(outputImageWidth) * outputImageHeight * outputChannels);
    for (size_t i = 0; i < outputImageData.size(); i += outputChannels) {
        outputImageData[i]     = bgColor[0];
        outputImageData[i + 1] = bgColor[1];
        outputImageData[i + 2] = bgColor[2];
    }

    // 4. 渲染字符
    int currentY = ascent; // 从第一个基线开始绘制 (stbtt 坐标系 y 向下为正)
    for (const string& line : asciiResultLines) {
        int currentX = 0;
        for (char c : line) {
            int char_w, char_h, xoff, yoff;
            // 获取字符的单色位图 (8bpp alpha)
            unsigned char* bitmap = stbtt_GetCodepointBitmap(&fontInfo, scale, scale, c, &char_w, &char_h, &xoff, &yoff);
             if (bitmap) {
                 // 获取字符的水平度量
                 stbtt_GetCodepointHMetrics(&fontInfo, c, &advanceWidth, &leftSideBearing);

                 // 计算绘制位置 (基线 + yoff)
                 int drawX_base = currentX + static_cast<int>(round(leftSideBearing * scale)) + xoff;
                 int drawY_base = currentY + yoff; // yoff 通常是负的或0

                 // 绘制位图到输出图像
                 for (int y = 0; y < char_h; ++y) {
                     for (int x = 0; x < char_w; ++x) {
                         int outX = drawX_base + x;
                         int outY = drawY_base + y;

                         // 检查边界
                         if (outX >= 0 && outX < outputImageWidth && outY >= 0 && outY < outputImageHeight) {
                             // 获取位图的 alpha 值 (0-255)
                             unsigned char alpha = bitmap[y * char_w + x];
                             if (alpha > 0) { // 只绘制非完全透明的部分
                                 // 简单的混合：用前景色替换背景色 (可以实现更复杂的alpha混合)
                                 size_t pixelIndex = (static_cast<size_t>(outY) * outputImageWidth + outX) * outputChannels;
                                 // 这里可以根据 alpha 值进行混合，但为了简单起见，只要 alpha > 0 就设为前景色
                                 if (alpha > 128) { // 或者用一个阈值
                                     outputImageData[pixelIndex]     = fgColor[0];
                                     outputImageData[pixelIndex + 1] = fgColor[1];
                                     outputImageData[pixelIndex + 2] = fgColor[2];
                                 }
                             }
                         }
                     }
                 }
                 // 释放 stbtt 分配的位图内存
                 stbtt_FreeBitmap(bitmap, nullptr); // 第二个参数是用户上下文，这里不需要

                 // 前进到下一个字符位置
                 currentX += static_cast<int>(round(advanceWidth * scale));
            } else {
                 // 如果某个字符无法渲染 (比如字体不支持)，也前进一点距离
                 stbtt_GetCodepointHMetrics(&fontInfo, ' ', &advanceWidth, &leftSideBearing); // 用空格宽度代替
                 currentX += static_cast<int>(round(advanceWidth * scale));
            }
        }
        currentY += lineHeight; // 移动到下一行
    }

    // 5. 保存图片
    if (!stbi_write_png(outputImagePath.c_str(), outputImageWidth, outputImageHeight, outputChannels, outputImageData.data(), outputImageWidth * outputChannels)) {
        cerr << "错误: 无法将ASCII艺术保存为图片 '" << outputImagePath << "'" << endl;
        // 字体和图像内存会自动释放
        return 1;
    }

    cout << "ASCII艺术已成功渲染并保存到: " << outputImagePath << endl;

    // stbi_image_free(imgData); // 已由 unique_ptr 管理
    // 字体 buffer 和 outputImageData 的 vector 会自动释放

    return 0; // Program finished successfully
}
