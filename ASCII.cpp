#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream> // For reading font file
#include <memory>  // For unique_ptr
#include <filesystem> // For path manipulation (needs C++17)

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

// Helper function to read a file into a vector of unsigned char
// Signature and internal types updated to reflect 'using namespace std'
vector<unsigned char> read_file(const string& filename) {
    // Use ifstream directly (from std namespace)
    ifstream file(filename, ios::binary | ios::ate);
    if (!file) {
        // Use cerr and endl directly
        cerr << "错误: 无法打开文件 '" << filename << "'" << endl;
        return {};
    }
    // Use streamsize directly
    streamsize size = file.tellg();
    file.seekg(0, ios::beg);
    // Use vector directly
    vector<unsigned char> buffer(static_cast<size_t>(size)); // 确保类型安全
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
         cerr << "错误: 无法读取文件 '" << filename << "'" << endl;
        return {};
    }
    return buffer;
}


int main() {
    // Use string directly
    string imagePath;
    const int targetWidth = 256; // 例如，设置为 256 个字符宽
    const double charAspectRatioCorrection = 2.0; // 字符高宽比修正
    const string fontPath = "C:\\Computer\\Code666\\github_cpp\\pics\\Consolas.ttf";
    const float fontSize = 15.0f;       // <--- 字体大小 (像素)
    const string baseOutputFilename = "output_ascii_art.png"; // 定义基础输出文件名
    const unsigned char bgColor[3] = {255, 255, 255}; // 背景色 (白色)
    const unsigned char fgColor[3] = {0, 0, 0};       // 前景色/文字颜色 (黑色)

    // Use cout, getline, cin directly
    cout << "请输入图片文件路径: ";
    getline(cin, imagePath); // Use getline for paths with spaces

    // --- 构造输出路径 ---
    // Use path directly (due to 'using std::filesystem::path;')
    path inputPathObj(imagePath);
    path outputDir = inputPathObj.parent_path(); // 获取父目录
    path finalOutputPathObj = outputDir / baseOutputFilename; // 组合路径
    // Use string directly
    string finalOutputPathString = finalOutputPathObj.string(); // 转换为字符串以供 stbi 使用

    // 打印将要使用的输出路径（用于调试或确认）
    cout << "输出文件将保存到: " << finalOutputPathString << endl;
    // --- 路径构造结束 ---

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
    // Use unique_ptr directly
    unique_ptr<unsigned char, void(*)(void*)> imgDataPtr(imgData, stbi_image_free);


    cout << "图片加载成功: " << width << "x" << height << ", 通道数: " << channels << endl;
    cout << "目标ASCII宽度 (来自变量): " << targetWidth << endl; // 明确宽度来源


    // Calculate target height based on target width and aspect ratio correction
    int targetHeight = static_cast<int>(round(static_cast<double>(height * targetWidth) / (width * charAspectRatioCorrection)));
    // Use max directly (from <algorithm> included in std)
    targetHeight = max(1, targetHeight); // Ensure height is at least 1

    cout << "计算出的ASCII高度: " << targetHeight << endl;


    // Define ASCII characters for grayscale levels (dark to light)
    const string asciiChars = "@%#*+=-:. "; // Common ramp (dense chars for dark areas)
    const int numAsciiChars = static_cast<int>(asciiChars.length());

    // Calculate scaling factors: how many original pixels map to one output character
    double xScale = static_cast<double>(width) / targetWidth;
    double yScale = static_cast<double>(height) / targetHeight;

    // --- 存储ASCII结果 ---
    // Use vector and string directly
    vector<string> asciiResultLines;
    asciiResultLines.reserve(targetHeight);

    // Iterate through the rows and columns of the target ASCII representation
    for (int yOut = 0; yOut < targetHeight; ++yOut) {
        string currentLine = "";
        currentLine.reserve(targetWidth);
        for (int xOut = 0; xOut < targetWidth; ++xOut) {

            // Use floor directly (from <cmath> included in std)
            int xImg = static_cast<int>(floor((xOut + 0.5) * xScale));
            int yImg = static_cast<int>(floor((yOut + 0.5) * yScale));

            // Use max and min directly (from <algorithm> included in std)
            xImg = max(0, min(xImg, width - 1));
            yImg = max(0, min(yImg, height - 1));

            size_t pixelOffset = (static_cast<size_t>(yImg) * width + xImg) * channels;

            unsigned char r = imgData[pixelOffset];
            unsigned char g = (channels > 1) ? imgData[pixelOffset + 1] : r;
            unsigned char b = (channels > 2) ? imgData[pixelOffset + 2] : r;

            int gray = (static_cast<int>(r) + g + b) / 3;

            // Use floor directly
            int asciiIndex = static_cast<int>(floor((gray / 255.0f) * (numAsciiChars - 1)));
            // Use max and min directly
            asciiIndex = max(0, min(asciiIndex, numAsciiChars - 1));

            currentLine += asciiChars[static_cast<size_t>(asciiIndex)];
        }
         asciiResultLines.push_back(currentLine);
         // cout << currentLine << endl; // Optional console preview
    }

    // --- 将ASCII结果渲染为图片 ---
    cout << "\n正在渲染ASCII艺术为图片..." << endl;

    // Use vector directly
    vector<unsigned char> fontBuffer = read_file(fontPath);
    if (fontBuffer.empty()) {
        return 1;
    }

    stbtt_fontinfo fontInfo;
    if (!stbtt_InitFont(&fontInfo, fontBuffer.data(), stbtt_GetFontOffsetForIndex(fontBuffer.data(), 0))) {
        cerr << "错误: 初始化字体失败。" << endl;
        return 1;
    }

    // Use round directly (from <cmath> included in std)
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
    const int outputChannels = 3; // RGB

    // Use vector directly
    vector<unsigned char> outputImageData(static_cast<size_t>(outputImageWidth) * outputImageHeight * outputChannels);
    for (size_t i = 0; i < outputImageData.size(); i += outputChannels) {
        outputImageData[i]     = bgColor[0];
        outputImageData[i + 1] = bgColor[1];
        outputImageData[i + 2] = bgColor[2];
    }

    int currentY = ascent;
    // Use string directly
    for (const string& line : asciiResultLines) {
        int currentX = 0;
        for (char c : line) {
            int char_w, char_h, xoff, yoff;
            unsigned char* bitmap = stbtt_GetCodepointBitmap(&fontInfo, scale, scale, c, &char_w, &char_h, &xoff, &yoff);
             if (bitmap) {
                 stbtt_GetCodepointHMetrics(&fontInfo, c, &advanceWidth, &leftSideBearing);
                 // Use round directly
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
                                 outputImageData[pixelIndex]     = fgColor[0];
                                 outputImageData[pixelIndex + 1] = fgColor[1];
                                 outputImageData[pixelIndex + 2] = fgColor[2];
                             }
                         }
                     }
                 }
                 stbtt_FreeBitmap(bitmap, nullptr);
                 // Use round directly
                 currentX += static_cast<int>(round(advanceWidth * scale));
            } else {
                 stbtt_GetCodepointHMetrics(&fontInfo, ' ', &advanceWidth, &leftSideBearing);
                 // Use round directly
                 currentX += static_cast<int>(round(advanceWidth * scale));
            }
        }
        currentY += lineHeight;
    }

    if (!stbi_write_png(finalOutputPathString.c_str(), outputImageWidth, outputImageHeight, outputChannels, outputImageData.data(), outputImageWidth * outputChannels)) {
        cerr << "错误: 无法将ASCII艺术保存为图片 '" << finalOutputPathString << "'" << endl;
        return 1;
    }

    cout << "ASCII艺术已成功渲染并保存到: " << finalOutputPathString << endl;

    return 0;
}
