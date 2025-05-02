#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream> // For reading font file
#include <memory>  // For unique_ptr
#include <filesystem> // For path manipulation and directory creation (needs C++17)
#include <system_error> // For filesystem error handling
#include <chrono>      // For high-precision timing
#include <iomanip>     // For formatting output (setprecision)

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
using namespace std::chrono; // Bring chrono members into scope

// --- Global Constants and Types ---
const string ASCII_CHARS = "@%#*+=-:. ";
const int NUM_ASCII_CHARS = static_cast<int>(ASCII_CHARS.length());
const int OUTPUT_CHANNELS = 3; // Output PNG as RGB

enum class ColorScheme {
    BLACK_ON_WHITE, WHITE_ON_BLACK, GREEN_ON_BLACK, PURPLE_ON_BLACK,
    COLOR_ON_WHITE, COLOR_ON_BLACK
};

struct CharColorInfo {
    char character;
    unsigned char color[3]; // R, G, B from original image
};

// --- Helper Functions ---

// Reads a file into a byte vector
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

// Gets a descriptive suffix for filenames based on the color scheme
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

// --- Core Functionality Modules ---

// 1. Get User Input
string getUserInputImagePath() {
    string imagePath;
    cout << "请输入图片文件路径: ";
    getline(cin, imagePath);
    // Basic check if path is empty
    if (imagePath.empty()) {
        cerr << "错误: 输入路径为空。" << endl;
    }
    // Add more validation if needed (e.g., check file existence here or later)
    return imagePath;
}

// 2. Setup Output Directory
path setupOutputDirectory(const path& inputPath, const string& subDirName) {
    path parentDir = inputPath.parent_path();
    path outputSubDirPath = parentDir / subDirName;
    try {
        if (filesystem::create_directory(outputSubDirPath)) {
            cout << "已创建输出子目录: " << outputSubDirPath.string() << endl;
        } else {
             if (!filesystem::exists(outputSubDirPath)) {
                 cerr << "错误: 无法创建输出子目录: " << outputSubDirPath.string() << endl;
                 return path(); // Return empty path on failure
             }
        }
        return outputSubDirPath; // Return the path to the subdirectory
    } catch (const filesystem::filesystem_error& e) {
        cerr << "错误: 创建输出子目录时发生异常: " << outputSubDirPath.string() << endl;
        cerr << "错误详情: " << e.what() << endl;
         if (!filesystem::is_directory(outputSubDirPath)) return path(); // Return empty path
         cerr << "警告: 尝试继续，假设目录已存在..." << endl;
         return outputSubDirPath; // Return path even if warning occurred but dir exists
    } catch (const std::exception& e) {
        cerr << "错误: 创建目录时发生未知异常: " << e.what() << endl;
        return path(); // Return empty path
    }
}

// 3. Load Input Image
// Returns unique_ptr to image data, sets width/height by reference.
unique_ptr<unsigned char, void(*)(void*)> loadImage(const string& imagePath, int& width, int& height) {
    unsigned char *data = stbi_load(imagePath.c_str(), &width, &height, nullptr, OUTPUT_CHANNELS); // Request desired channels
    if (data == nullptr) {
        cerr << "错误: 无法加载图片 '" << imagePath << "' (或无法转换为RGB)" << endl;
        cerr << "STB Image 错误原因: " << stbi_failure_reason() << endl;
        return unique_ptr<unsigned char, void(*)(void*)>(nullptr, stbi_image_free); // Return null unique_ptr
    }
    cout << "图片加载成功 (强制RGB): " << width << "x" << height << ", 通道数: " << OUTPUT_CHANNELS << endl;
    return unique_ptr<unsigned char, void(*)(void*)>(data, stbi_image_free);
}

// 4. Generate ASCII Data
vector<vector<CharColorInfo>> generateAsciiData(const unsigned char* imgData, int width, int height, int targetWidth, int targetHeight)
{
    cout << "\n正在生成ASCII数据..." << endl;
    vector<vector<CharColorInfo>> asciiResultData;
    if (!imgData || width <= 0 || height <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        cerr << "错误：无效的参数传递给 generateAsciiData。" << endl;
        return asciiResultData; // Return empty vector
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
            // Corrected mapping: Use NUM_ASCII_CHARS, not NUM_ASCII_CHARS - 1 in division part
            int asciiIndex = static_cast<int>(floor((gray / 256.0f) * NUM_ASCII_CHARS));
            asciiIndex = max(0, min(asciiIndex, NUM_ASCII_CHARS - 1)); // Clamp index

            CharColorInfo info;
            info.character = ASCII_CHARS[static_cast<size_t>(asciiIndex)];
            info.color[0] = r; info.color[1] = g; info.color[2] = b;
            currentLineData.push_back(info);
        }
        asciiResultData.push_back(currentLineData);
    }
    // Moved print statement after function finishes
    // cout << "ASCII数据生成完毕。" << endl;
    return asciiResultData;
}

// 5. Load Font Info
// fontBuffer is passed by reference to keep the data alive.
bool loadFontInfo(const string& fontPath, stbtt_fontinfo& fontInfo, vector<unsigned char>& fontBuffer) {
    cout << "\n正在加载字体..." << endl;
    fontBuffer = read_file(fontPath);
    if (fontBuffer.empty()) return false; // read_file prints error

    if (!stbtt_InitFont(&fontInfo, fontBuffer.data(), stbtt_GetFontOffsetForIndex(fontBuffer.data(), 0))) {
        cerr << "错误: 初始化字体失败。" << endl;
        return false;
    }
    cout << "字体加载成功。" << endl;
    return true;
}

// 6. Calculate Output Dimensions & Font Metrics
// Outputs calculated values via references.
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

    // Estimate char width using 'M'
    int advanceWidth, leftSideBearing;
    stbtt_GetCodepointHMetrics(&fontInfo, 'M', &advanceWidth, &leftSideBearing);
    charWidth = static_cast<int>(round(advanceWidth * scale));
    // Ensure minimum char width to avoid zero division or zero size image later
    if (charWidth <= 0) {
        cerr << "警告: 计算出的字符宽度为零或负数，使用默认值 1。" << endl;
        charWidth = 1;
    }


    outputImageWidth = targetWidth * charWidth;
    outputImageHeight = targetHeight * lineHeight;

    // Ensure minimum dimensions
    outputImageWidth = max(1, outputImageWidth);
    outputImageHeight = max(1, outputImageHeight);

    cout << "输出图像尺寸计算完毕: " << outputImageWidth << "x" << outputImageHeight << endl;
}

// 7. Set Scheme Colors
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
            // fgColor is unused for color schemes, but set defaults
            fgColor[0] = 0;   fgColor[1] = 0;   fgColor[2] = 0;
            break;
        case ColorScheme::COLOR_ON_BLACK:
            bgColor[0] = 0;   bgColor[1] = 0;   bgColor[2] = 0;
            // fgColor is unused for color schemes, but set defaults
            fgColor[0] = 255; fgColor[1] = 255; fgColor[2] = 255;
            break;
        case ColorScheme::BLACK_ON_WHITE:
        default:
            bgColor[0] = 255; bgColor[1] = 255; bgColor[2] = 255;
            fgColor[0] = 0;   fgColor[1] = 0;   fgColor[2] = 0;
            break;
    }
}

// 8. Render ASCII Art onto an image buffer
// Modifies outputImageData in place.
void renderAsciiArt(vector<unsigned char>& outputImageData,
                    const vector<vector<CharColorInfo>>& asciiData,
                    const stbtt_fontinfo& fontInfo,
                    ColorScheme scheme,
                    const unsigned char bgColor[3], // Background color for this scheme
                    const unsigned char fgColor[3], // Fixed foreground color (if used)
                    int imgWidth, int imgHeight,
                    int ascent, float scale, int lineHeight)
{
    // Fill background first
    // Optimization: Use std::fill or similar if faster for huge buffers,
    // but direct loop is usually fine and clear.
    for (size_t i = 0; i < outputImageData.size(); i += OUTPUT_CHANNELS) {
        outputImageData[i]     = bgColor[0];
        outputImageData[i + 1] = bgColor[1];
        outputImageData[i + 2] = bgColor[2];
    }

    // Render characters
    int currentY = ascent; // Start rendering from the first baseline
    for (const auto& lineData : asciiData) {
        int currentX = 0;
        for (const auto& charInfo : lineData) {
            char c = charInfo.character;
            int char_w, char_h, xoff, yoff;
            unsigned char* bitmap = stbtt_GetCodepointBitmap(&fontInfo, scale, scale, c, &char_w, &char_h, &xoff, &yoff);

            int advanceWidth, leftSideBearing; // Need advance width for positioning
            stbtt_GetCodepointHMetrics(&fontInfo, c, &advanceWidth, &leftSideBearing);
            int charAdvance = static_cast<int>(round(advanceWidth * scale));


            if (bitmap) {
                int drawX_base = currentX + static_cast<int>(round(leftSideBearing * scale)) + xoff;
                int drawY_base = currentY + yoff;

                for (int y = 0; y < char_h; ++y) {
                    for (int x = 0; x < char_w; ++x) {
                        int outX = drawX_base + x;
                        int outY = drawY_base + y;
                        // Bounds check before accessing outputImageData
                        if (outX >= 0 && outX < imgWidth && outY >= 0 && outY < imgHeight) {
                            unsigned char alpha = bitmap[y * char_w + x];
                            if (alpha > 128) { // If pixel is significantly visible
                                size_t pixelIndex = (static_cast<size_t>(outY) * imgWidth + outX) * OUTPUT_CHANNELS;

                                // Choose color based on scheme
                                const unsigned char* finalColor = nullptr;
                                if (scheme == ColorScheme::COLOR_ON_WHITE || scheme == ColorScheme::COLOR_ON_BLACK) {
                                    finalColor = charInfo.color;
                                } else {
                                    finalColor = fgColor;
                                }

                                // --- Alpha Blending (Optional but better) ---
                                // Formula: Out = Src * Alpha + Dst * (1 - Alpha)
                                // Here Alpha is bitmap[y*char_w+x] / 255.0f
                                float alphaF = alpha / 255.0f;
                                outputImageData[pixelIndex]     = static_cast<unsigned char>(finalColor[0] * alphaF + outputImageData[pixelIndex]     * (1.0f - alphaF));
                                outputImageData[pixelIndex + 1] = static_cast<unsigned char>(finalColor[1] * alphaF + outputImageData[pixelIndex + 1] * (1.0f - alphaF));
                                outputImageData[pixelIndex + 2] = static_cast<unsigned char>(finalColor[2] * alphaF + outputImageData[pixelIndex + 2] * (1.0f - alphaF));
                                // --- End Alpha Blending ---

                                // --- Original Simple Overwrite (commented out) ---
                                // outputImageData[pixelIndex]     = finalColor[0];
                                // outputImageData[pixelIndex + 1] = finalColor[1];
                                // outputImageData[pixelIndex + 2] = finalColor[2];
                                // --- End Simple Overwrite ---
                            }
                        }
                    }
                }
                stbtt_FreeBitmap(bitmap, nullptr);
            } else {
                 // Handle missing glyph if needed, advanceWidth already calculated
                 // cerr << "Warning: Glyph not found for character '" << c << "'" << endl;
            }
             currentX += charAdvance; // Advance cursor using calculated advance width
        }
        currentY += lineHeight; // Move to next line
    }
}

// 9. Save Image
bool saveImage(const path& outputPath, int width, int height, int channels, const vector<unsigned char>& data) {
    // Check if data vector has the expected size
    if (data.size() != static_cast<size_t>(width) * height * channels) {
         cerr << "错误: 传递给 saveImage 的数据大小 (" << data.size()
              << ") 与预期大小 (" << (static_cast<size_t>(width) * height * channels)
              << ") 不匹配。" << endl;
         return false;
    }
    if (width <= 0 || height <= 0) {
         cerr << "错误: 传递给 saveImage 的宽度或高度无效 (" << width << "x" << height << ")" << endl;
         return false;
    }

    if (!stbi_write_png(outputPath.string().c_str(), width, height, channels, data.data(), width * channels)) {
        cerr << "错误: 无法将图像保存到 '" << outputPath.string() << "'" << endl;
        // Potentially add more error info if stb_image_write provides it
        return false;
    }
    return true;
}


// --- Main Application Logic ---
int main() {
    // --- Configuration ---
    const int targetWidth = 2048; // Reduced for quicker testing if needed
    const double charAspectRatioCorrection = 2.0;
    const string fontPath = "C:\\Windows\\Fonts\\consola.ttf"; // Example: Windows path, adjust as needed
    // Alternative Linux path example: "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
    const float fontSize = 15.0f;
    const string baseOutputFilename = "output_ascii_art";
    const string outputExtension = ".png";
    const string outputSubDirName = "ascii_output";

    const vector<ColorScheme> schemesToGenerate = {
        //ColorScheme::BLACK_ON_WHITE,
        //ColorScheme::WHITE_ON_BLACK,
        //ColorScheme::GREEN_ON_BLACK,
        ColorScheme::COLOR_ON_WHITE,
        ColorScheme::COLOR_ON_BLACK
    };
    // --- End Configuration ---

    // --- Step 1: Get Input ---
    string imagePathStr = getUserInputImagePath();
    if (imagePathStr.empty()) return 1;
    path imagePath(imagePathStr); // Convert to path object

    // Basic check if input file exists
     if (!filesystem::exists(imagePath) || !filesystem::is_regular_file(imagePath)) {
         cerr << "错误: 输入图片文件不存在或不是一个常规文件: " << imagePath.string() << endl;
         return 1;
     }


    // --- Step 2: Setup Output ---
    path outputSubDirPath = setupOutputDirectory(imagePath, outputSubDirName);
    if (outputSubDirPath.empty()) return 1; // Check if setup failed

    // --- Step 3: Load Image ---
    cout << "\n开始加载图片..." << endl;
    auto load_start = high_resolution_clock::now();
    int width, height;
    auto imgDataPtr = loadImage(imagePath.string(), width, height); // unique_ptr manages memory
    auto load_end = high_resolution_clock::now();
    auto load_duration = duration_cast<milliseconds>(load_end - load_start);
     cout << "-> 图片加载用时: " << fixed << setprecision(3) << load_duration.count() / 1000.0 << " 秒" << endl;
    if (!imgDataPtr) return 1;


    // --- Start Total Processing Timer ---
    auto total_processing_start = high_resolution_clock::now();

    // --- Step 4: Generate ASCII Data ---
    auto ascii_conv_start = high_resolution_clock::now(); // <-- ASCII Start Time
    int targetHeight = static_cast<int>(round(static_cast<double>(height * targetWidth) / (width * charAspectRatioCorrection)));
    targetHeight = max(1, targetHeight); // Ensure height is at least 1
    cout << "计算出的ASCII高度: " << targetHeight << endl;

    vector<vector<CharColorInfo>> asciiResultData = generateAsciiData(
        imgDataPtr.get(), width, height, targetWidth, targetHeight);
    auto ascii_conv_end = high_resolution_clock::now(); // <-- ASCII End Time
    if (asciiResultData.empty()) return 1; // Check if data generation failed
    auto ascii_conv_duration = duration_cast<milliseconds>(ascii_conv_end - ascii_conv_start);
    cout << "-> ASCII 数据转换用时: " << fixed << setprecision(3) << ascii_conv_duration.count() / 1000.0 << " 秒" << endl;


    // --- Step 5: Load Font ---
    // Note: Font loading time is measured separately if needed, but done here for setup
    stbtt_fontinfo fontInfo;
    vector<unsigned char> fontBuffer; // Keep buffer alive for fontInfo
    if (!loadFontInfo(fontPath, fontInfo, fontBuffer)) return 1;


    // --- Step 6: Calculate Dimensions ---
    int charWidth, lineHeight, outputImageWidth, outputImageHeight, ascent, descent, lineGap;
    float scale;
    calculateOutputDimensions(fontInfo, fontSize, targetWidth, targetHeight,
                              charWidth, lineHeight, outputImageWidth, outputImageHeight,
                              scale, ascent, descent, lineGap);
     // Add check for zero dimensions calculated
     if (outputImageWidth <= 0 || outputImageHeight <= 0) {
         cerr << "错误: 计算出的输出图像尺寸无效 (" << outputImageWidth << "x" << outputImageHeight << ")" << endl;
         return 1;
     }


    // --- Step 7: Process Each Selected Scheme ---
    cout << "\n开始处理选定的颜色方案:" << endl;
    for (const auto& currentScheme : schemesToGenerate) {

        auto theme_start = high_resolution_clock::now(); // <-- Theme Start Time

        string schemeSuffix = getSchemeSuffix(currentScheme);
        string currentOutputFilename = baseOutputFilename + schemeSuffix + outputExtension;
        path finalOutputPathObj = outputSubDirPath / currentOutputFilename;

        cout << "  正在处理方案: " << schemeSuffix.substr(1)
             << ", 输出到: " << finalOutputPathObj.string() << endl;

        // a. Set Colors
        unsigned char bgColor[3];
        unsigned char fgColor[3];
        setSchemeColors(currentScheme, bgColor, fgColor);

        // b. Create Buffer for this scheme
        vector<unsigned char> outputImageData;
         try {
             outputImageData.resize(static_cast<size_t>(outputImageWidth) * outputImageHeight * OUTPUT_CHANNELS);
         } catch (const std::bad_alloc& e) {
             cerr << "错误: 分配输出图像缓冲区失败 (大小: " << outputImageWidth << "x" << outputImageHeight << ")。内存不足？" << endl;
             cerr << e.what() << endl;
             continue; // Skip this theme
         }


        // c. Render Art (PNG Creation Part 1)
        auto render_start = high_resolution_clock::now(); // <-- Render Start Time
        renderAsciiArt(outputImageData, asciiResultData, fontInfo, currentScheme,
                       bgColor, fgColor, outputImageWidth, outputImageHeight,
                       ascent, scale, lineHeight);
        auto render_end = high_resolution_clock::now(); // <-- Render End Time
        auto render_duration = duration_cast<milliseconds>(render_end - render_start);
        cout << "    -> 渲染用时: " << fixed << setprecision(3) << render_duration.count() / 1000.0 << " 秒" << endl;


        // d. Save Image (PNG Creation Part 2)
         auto save_start = high_resolution_clock::now();
        if (!saveImage(finalOutputPathObj, outputImageWidth, outputImageHeight, OUTPUT_CHANNELS, outputImageData)) {
            cerr << "  错误: 保存方案 " << schemeSuffix.substr(1) << " 失败。" << endl;
            // Decide whether to continue or stop: continue;
        } else {
             auto save_end = high_resolution_clock::now();
             auto save_duration = duration_cast<milliseconds>(save_end - save_start);
             cout << "    -> 保存用时: " << fixed << setprecision(3) << save_duration.count() / 1000.0 << " 秒" << endl;
            cout << "  方案 " << schemeSuffix.substr(1) << " 已成功保存。" << endl;
        }

        auto theme_end = high_resolution_clock::now(); // <-- Theme End Time
        auto theme_duration = duration_cast<milliseconds>(theme_end - theme_start);
         cout << "  -> 单个主题处理总用时: " << fixed << setprecision(3) << theme_duration.count() / 1000.0 << " 秒" << endl;

    } // --- End Scheme Loop ---

    auto total_processing_end = high_resolution_clock::now(); // <-- Total Processing End Time
    auto total_processing_duration = duration_cast<milliseconds>(total_processing_end - total_processing_start);

    cout << "\n所有选定的颜色方案处理完毕。" << endl;
    cout << "-> 总核心处理用时: " << fixed << setprecision(3) << total_processing_duration.count() / 1000.0 << " 秒" << endl;

    return 0;
}
