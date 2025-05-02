#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <filesystem> // C++17 for path manipulation
#include <fstream>   // Not strictly needed now, but good practice

// Define these macros so stb_image*.h includes the implementation
// Make sure these definitions only appear in one .cpp file
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // Assume stb_image.h is in the same directory or include path

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h" // Assume stb_image_write.h is here too

using namespace std;
namespace fs = std::filesystem; // Alias for convenience

int main() {
    string imagePath;

    // --- 配置区域 ---
    const int targetWidth = 80; // 例如，设置为 80 个字符宽
    const double charAspectRatioCorrection = 2.0; // 字符高宽比修正
    const string asciiChars = "@%#*+=-:. "; // 灰度到ASCII字符映射 (从暗到亮)
    // --- 配置区域结束 ---

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

    cout << "图片加载成功: " << width << "x" << height << ", 通道数: " << channels << endl;
    cout << "目标ASCII宽度 (来自变量): " << targetWidth << endl; // 明确宽度来源

    // Calculate target height based on target width and aspect ratio correction
    int targetHeight = static_cast<int>(round(static_cast<double>(height * targetWidth) / (width * charAspectRatioCorrection)));
    targetHeight = max(1, targetHeight); // Ensure height is at least 1

    cout << "计算出的ASCII高度: " << targetHeight << endl;

    const int numAsciiChars = asciiChars.length();

    // Calculate scaling factors: how many original pixels map to one output character/pixel cell
    double xScale = static_cast<double>(width) / targetWidth;
    double yScale = static_cast<double>(height) / targetHeight;

    // --- 新增: 用于存储ASCII艺术的字符串向量和用于输出图像的灰度像素数据 ---
    vector<string> asciiArtRows(targetHeight);
    vector<unsigned char> outputGrayscalePixels(targetWidth * targetHeight); // Grayscale image (1 channel)

    // Iterate through the rows and columns of the target ASCII/pixel representation
    for (int yOut = 0; yOut < targetHeight; ++yOut) {
        string currentRow = ""; // Build the row for console output
        for (int xOut = 0; xOut < targetWidth; ++xOut) {

            // --- Find the *average* color in the corresponding region of the original image ---
            // This gives a better representation than just sampling one pixel
            double totalGray = 0;
            int pixelsSampled = 0;

            // Define the bounding box in the original image for this output cell
            int startX = static_cast<int>(floor(xOut * xScale));
            int endX = static_cast<int>(floor((xOut + 1) * xScale));
            int startY = static_cast<int>(floor(yOut * yScale));
            int endY = static_cast<int>(floor((yOut + 1) * yScale));

            // Clamp bounds to image dimensions
            endX = min(endX, width);
            endY = min(endY, height);

            for (int yImg = startY; yImg < endY; ++yImg) {
                for (int xImg = startX; xImg < endX; ++xImg) {
                     // Calculate the index for the pixel in the image data array
                    unsigned long long pixelOffset = (static_cast<unsigned long long>(yImg) * width + xImg) * channels;

                    // Get the R, G, B values
                    unsigned char r = imgData[pixelOffset];
                    unsigned char g = (channels > 1) ? imgData[pixelOffset + 1] : r;
                    unsigned char b = (channels > 2) ? imgData[pixelOffset + 2] : r;

                    // Calculate grayscale intensity (using luminosity formula - often better than simple average)
                    // double grayValue = 0.2126 * r + 0.7152 * g + 0.0722 * b;
                    // Or stick to simple average if preferred:
                    double grayValue = (r + g + b) / 3.0;

                    totalGray += grayValue;
                    pixelsSampled++;
                }
            }

            int avgGray = 0;
            if (pixelsSampled > 0) {
                 avgGray = static_cast<int>(round(totalGray / pixelsSampled));
            }
             avgGray = max(0, min(avgGray, 255)); // Clamp average gray value


            // --- Store the average gray value for the output image ---
            // The output image will have dimensions targetWidth x targetHeight
            // Each "pixel" in our output image corresponds to one ASCII character position
            outputGrayscalePixels[yOut * targetWidth + xOut] = static_cast<unsigned char>(avgGray);


            // --- Select and store the ASCII character for console output ---
            // Map average grayscale value (0-255) to an ASCII character index
            int asciiIndex = static_cast<int>(floor((avgGray / 255.0) * (numAsciiChars - 1)));
             asciiIndex = max(0, min(asciiIndex, numAsciiChars - 1)); // Clamp index

            // Append the corresponding ASCII character to the current row string
            currentRow += asciiChars[asciiIndex];
        }
        // Store the completed row
        asciiArtRows[yOut] = currentRow;
    }

    // --- 输出 ASCII 艺术到控制台 ---
    cout << "\n--- ASCII Art Output ---" << endl;
    for (const string& row : asciiArtRows) {
        cout << row << endl;
    }
    cout << "--- End of ASCII Art ---" << endl;


    // --- 新增: 保存灰度图 ---
    cout << "\n正在尝试保存灰度图像..." << endl;

    // 1. 确定输出路径
    fs::path inputPathObj(imagePath);
    string outputFilename = inputPathObj.stem().string() + "_ascii_out.png"; // e.g., "myImage_ascii_out.png"

    // 默认保存在当前工作目录下 (通常是程序运行的目录)
    fs::path outputPath = fs::current_path() / outputFilename;

    // 如果你想强制保存在输入图片所在的目录下 (需要确保有写权限):
    // if (inputPathObj.has_parent_path()) {
    //     outputPath = inputPathObj.parent_path() / outputFilename;
    // } else {
    //     outputPath = fs::current_path() / outputFilename; // Fallback to current dir
    // }


    // 2. 调用 stbi_write_png 保存
    // 参数: filename, width, height, comp (components, 1 for grayscale), data, stride (bytes per row)
    int stride_in_bytes = targetWidth * sizeof(unsigned char); // = targetWidth for 1 channel unsigned char
    int success = stbi_write_png(outputPath.string().c_str(), targetWidth, targetHeight, 1, outputGrayscalePixels.data(), stride_in_bytes);

    // 3. 检查结果
    if (success) {
        cout << "灰度图像已成功保存到: " << outputPath.string() << endl;
    } else {
        cerr << "错误: 无法保存灰度图像到 " << outputPath.string() << endl;
        // 注意: stb_image_write 没有像 stb_image 那样的 stbi_failure_reason()
    }


    // Free the memory loaded by stb_image
    stbi_image_free(imgData);

    return 0; // Program finished successfully
}