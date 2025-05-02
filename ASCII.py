#include <iostream>

#include <string>

#include <vector>

#include <cmath>     

#include <algorithm> 



// Define this macro so stb_image.h includes the implementation

// Make sure this definition only appears in one .cpp file

#define STB_IMAGE_IMPLEMENTATION

#include "stb_image.h" // Assume stb_image.h is in the same directory or include path





using namespace std;







int main() {

    string imagePath;



    const int targetWidth = 80; // 例如，设置为 80 个字符宽



    cout << "请输入图片文件路径: ";

    getline(cin, imagePath); // Use getline for paths with spaces

    // --- 配置区域结束 ---





    // Character aspect ratio (height / width) - adjust if needed for your terminal font

    // Common values are between 1.8 and 2.2

    const double charAspectRatioCorrection = 2.0;



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





    // Define ASCII characters for grayscale levels (dark to light)

    const string asciiChars = "@%#*+=-:. "; // Common ramp (dense chars for dark areas)

    const int numAsciiChars = asciiChars.length();



    // Calculate scaling factors: how many original pixels map to one output character

    double xScale = static_cast<double>(width) / targetWidth;

    double yScale = static_cast<double>(height) / targetHeight;





    // Iterate through the rows and columns of the target ASCII representation

    for (int yOut = 0; yOut < targetHeight; ++yOut) {

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





            // Output the corresponding ASCII character

            cout << asciiChars[asciiIndex];

        }

        // Newline after each row of the ASCII output

        cout << endl;

    }



    // Free the memory loaded by stb_image

    stbi_image_free(imgData);



    return 0; // Program finished successfully

}
