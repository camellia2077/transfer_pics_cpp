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
    const int targetWidth = 256;
    const double charAspectRatioCorrection = 2.0;
    const string fontPath = "C:\\Computer\\Code666\\github_cpp\\pics\\Consolas.ttf";
    const float fontSize = 15.0f;
    const string baseOutputFilename = "output_ascii_art.png";
    // --- 定义要创建的子目录名称 ---
    // 注意：使用非 ASCII 字符如 "莉" 可能导致跨平台或编码问题，推荐使用 ASCII 名称
    const string outputSubDirName = "ascii_output";
    // const string outputSubDirName = "莉"; // 如果你坚持使用，请注意风险
    // --- 子目录名称定义结束 ---
    const unsigned char bgColor[3] = {255, 255, 255};
    const unsigned char fgColor[3] = {0, 0, 0};

    cout << "请输入图片文件路径: ";
    getline(cin, imagePath);

    // --- 修改：构造输出路径并创建子目录 ---
    path inputPathObj(imagePath);
    path parentDir = inputPathObj.parent_path(); // 获取父目录

    // 构造子目录的完整路径
    path outputSubDirPath = parentDir / outputSubDirName;

    // 尝试创建子目录
    try {
        // create_directory 如果目录已存在，则返回 false 且不抛出异常
        // 如果创建失败（例如权限问题），则抛出 filesystem_error
        if (filesystem::create_directory(outputSubDirPath)) {
            cout << "已创建输出子目录: " << outputSubDirPath.string() << endl;
        } else {
             // 目录已存在或创建失败但未抛出异常（理论上不应发生，除非并发）
             if (!filesystem::exists(outputSubDirPath)) {
                 // 如果目录不存在且创建失败，则报告错误
                 cerr << "错误: 无法创建输出子目录，但目录似乎也不存在: " << outputSubDirPath.string() << endl;
                 return 1; // 无法继续
             }
             // 如果目录已存在，则不需额外提示，继续执行
        }
    } catch (const filesystem::filesystem_error& e) {
        cerr << "错误: 创建输出子目录时发生异常: " << outputSubDirPath.string() << endl;
        cerr << "错误详情: " << e.what() << endl;
        // 检查是否是因为目录已存在（虽然 create_directory 不应因此抛出）
        if (!filesystem::is_directory(outputSubDirPath)) {
             return 1; // 如果不是目录且创建失败，则无法继续
        }
         // 如果是因为其他原因（如权限），但目录确实是存在的，或许可以尝试继续？
         // 这里选择退出更安全
         // return 1;
         // 或者仅打印警告并尝试继续（如果确定目录已存在）
         cerr << "警告: 尝试继续，假设目录已存在..." << endl;


    } catch (const std::exception& e) { // 捕获其他可能的标准异常
        cerr << "错误: 创建目录时发生未知异常: " << e.what() << endl;
        return 1;
    }


    // 组合最终的文件路径（在子目录内）
    path finalOutputPathObj = outputSubDirPath / baseOutputFilename;
    string finalOutputPathString = finalOutputPathObj.string();

    cout << "输出文件将保存到: " << finalOutputPathString << endl;
    // --- 路径构造和目录创建结束 ---

    if (targetWidth <= 0) {
        cerr << "错误: 配置的 targetWidth (" << targetWidth << ") 必须是正整数。" << endl;
        return 1;
    }

    int width, height, channels;
    unsigned char *imgData = stbi_load(imagePath.c_str(), &width, &height, &channels, 0);

    if (imgData == nullptr) {
        cerr << "错误: 无法加载图片 '" << imagePath << "'" << endl;
        cerr << "STB Image 错误原因: " << stbi_failure_reason() << endl;
        return 1;
    }
    unique_ptr<unsigned char, void(*)(void*)> imgDataPtr(imgData, stbi_image_free);

    cout << "图片加载成功: " << width << "x" << height << ", 通道数: " << channels << endl;
    cout << "目标ASCII宽度 (来自变量): " << targetWidth << endl;

    int targetHeight = static_cast<int>(round(static_cast<double>(height * targetWidth) / (width * charAspectRatioCorrection)));
    targetHeight = max(1, targetHeight);

    cout << "计算出的ASCII高度: " << targetHeight << endl;

    const string asciiChars = "@%#*+=-:. ";
    const int numAsciiChars = static_cast<int>(asciiChars.length());

    double xScale = static_cast<double>(width) / targetWidth;
    double yScale = static_cast<double>(height) / targetHeight;

    vector<string> asciiResultLines;
    asciiResultLines.reserve(targetHeight);

    for (int yOut = 0; yOut < targetHeight; ++yOut) {
        string currentLine = "";
        currentLine.reserve(targetWidth);
        for (int xOut = 0; xOut < targetWidth; ++xOut) {
            int xImg = static_cast<int>(floor((xOut + 0.5) * xScale));
            int yImg = static_cast<int>(floor((yOut + 0.5) * yScale));
            xImg = max(0, min(xImg, width - 1));
            yImg = max(0, min(yImg, height - 1));
            size_t pixelOffset = (static_cast<size_t>(yImg) * width + xImg) * channels;
            unsigned char r = imgData[pixelOffset];
            unsigned char g = (channels > 1) ? imgData[pixelOffset + 1] : r;
            unsigned char b = (channels > 2) ? imgData[pixelOffset + 2] : r;
            int gray = (static_cast<int>(r) + g + b) / 3;
            int asciiIndex = static_cast<int>(floor((gray / 255.0f) * (numAsciiChars - 1)));
            asciiIndex = max(0, min(asciiIndex, numAsciiChars - 1));
            currentLine += asciiChars[static_cast<size_t>(asciiIndex)];
        }
         asciiResultLines.push_back(currentLine);
    }

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
    for (size_t i = 0; i < outputImageData.size(); i += outputChannels) {
        outputImageData[i]     = bgColor[0];
        outputImageData[i + 1] = bgColor[1];
        outputImageData[i + 2] = bgColor[2];
    }

    int currentY = ascent;
    for (const string& line : asciiResultLines) {
        int currentX = 0;
        for (char c : line) {
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
                                 outputImageData[pixelIndex]     = fgColor[0];
                                 outputImageData[pixelIndex + 1] = fgColor[1];
                                 outputImageData[pixelIndex + 2] = fgColor[2];
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

    // --- 使用最终的路径保存文件 ---
    if (!stbi_write_png(finalOutputPathString.c_str(), outputImageWidth, outputImageHeight, outputChannels, outputImageData.data(), outputImageWidth * outputChannels)) {
        cerr << "错误: 无法将ASCII艺术保存为图片 '" << finalOutputPathString << "'" << endl;
        return 1;
    }

    cout << "ASCII艺术已成功渲染并保存到: " << finalOutputPathString << endl;

    return 0;
}
