// image_converter.cpp
//将输入的图像文件转换为字符画的中间表示形式，即 vector<vector<CharColorInfo>>。
#include "image_converter.h"
#include <iostream>
#include <memory> // For unique_ptr
#include <cmath>
#include <algorithm> // For std::max, std::min

// Define STB_IMAGE_IMPLEMENTATION in exactly one .cpp file
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // Image loading functions

namespace { // Use an anonymous namespace for internal helper functions

// Loads an image using stb_image
// Returns a unique_ptr to the data, and sets width/height.
// Returns nullptr on failure.
std::unique_ptr<unsigned char, void(*)(void*)> loadImage(const std::string& imagePath, int& width, int& height) {
    unsigned char *data = stbi_load(imagePath.c_str(), &width, &height, nullptr, OUTPUT_CHANNELS);
    if (data == nullptr) {
        std::cerr << "Error: Failed to load image '" << imagePath << "'. Reason: " << stbi_failure_reason() << std::endl;
        return std::unique_ptr<unsigned char, void(*)(void*)>(nullptr, stbi_image_free);
    }
    return std::unique_ptr<unsigned char, void(*)(void*)>(data, stbi_image_free);
}

// Generates the ASCII data structure from raw image pixel data
vector<vector<CharColorInfo>> generateAsciiData(const unsigned char* imgData, int width, int height, int targetWidth, int targetHeight) {
    vector<vector<CharColorInfo>> asciiResultData;
    if (!imgData || width <= 0 || height <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        std::cerr << "Error: Invalid arguments to generateAsciiData." << std::endl;
        return asciiResultData; // Return empty vector
    }

    asciiResultData.reserve(targetHeight);
    double xScale = static_cast<double>(width) / targetWidth;
    double yScale = static_cast<double>(height) / targetHeight;

    for (int yOut = 0; yOut < targetHeight; ++yOut) {
        vector<CharColorInfo> currentLineData;
        currentLineData.reserve(targetWidth);
        for (int xOut = 0; xOut < targetWidth; ++xOut) {
            // Use nearest neighbor sampling for simplicity (matches original code)
            int xImg = static_cast<int>(std::floor((xOut + 0.5) * xScale));
            int yImg = static_cast<int>(std::floor((yOut + 0.5) * yScale));

            // Clamp coordinates to be within image bounds
            xImg = std::max(0, std::min(xImg, width - 1));
            yImg = std::max(0, std::min(yImg, height - 1));

            size_t pixelOffset = (static_cast<size_t>(yImg) * width + xImg) * OUTPUT_CHANNELS;
            unsigned char r = imgData[pixelOffset];
            unsigned char g = imgData[pixelOffset + 1];
            unsigned char b = imgData[pixelOffset + 2];

            // Calculate grayscale intensity
            int gray = (static_cast<int>(r) + g + b) / 3;

            // Map intensity to ASCII character index
            int asciiIndex = static_cast<int>(std::floor((gray / 255.0f) * (NUM_ASCII_CHARS - 1)));
            asciiIndex = std::max(0, std::min(asciiIndex, NUM_ASCII_CHARS - 1)); // Clamp index

            CharColorInfo info;
            info.character = ASCII_CHARS[static_cast<size_t>(asciiIndex)];
            info.color[0] = r; info.color[1] = g; info.color[2] = b;
            currentLineData.push_back(info);
        }
        asciiResultData.push_back(std::move(currentLineData)); // Move vector
    }
    return asciiResultData;
}

} // end anonymous namespace

// --- Public Function Implementation ---

std::optional<AsciiConversionResult> convertImageToAscii(
    const std::filesystem::path& imagePath,
    int targetAsciiWidth,
    double aspectRatioCorrection)
{
    std::cout << "Loading image " << imagePath.filename().string() << "..." << std::endl;
    int width, height;
    auto imgDataPtr = loadImage(imagePath.string(), width, height);

    if (!imgDataPtr) {
        return std::nullopt; // Failed to load image
    }
     std::cout << "-> Loaded (" << width << "x" << height << ")" << std::endl;


    std::cout << "Generating ASCII data..." << std::endl;
    // Calculate target height based on width and aspect ratio correction
    int targetAsciiHeight = static_cast<int>(std::round(static_cast<double>(height * targetAsciiWidth) / (width * aspectRatioCorrection)));
    targetAsciiHeight = std::max(1, targetAsciiHeight); // Ensure at least 1 row
     std::cout << "Calculated ASCII grid: " << targetAsciiWidth << "x" << targetAsciiHeight << std::endl;


    vector<vector<CharColorInfo>> asciiData = generateAsciiData(
        imgDataPtr.get(), width, height, targetAsciiWidth, targetAsciiHeight);

    if (asciiData.empty() || asciiData[0].empty()) {
        std::cerr << "Error: Failed to generate ASCII data for " << imagePath.filename().string() << "." << std::endl;
        return std::nullopt;
    }

    AsciiConversionResult result;
    result.data = std::move(asciiData);
    result.originalWidth = width;
    result.originalHeight = height;
    result.asciiWidth = targetAsciiWidth;
    result.asciiHeight = targetAsciiHeight;

    return result;
}
