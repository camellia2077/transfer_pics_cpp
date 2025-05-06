// ascii_renderer.cpp
//将 image_converter 生成的字符画中间表示
#include "ascii_renderer.h"
#include <iostream>
#include <vector>
#include <cmath> // For std::round
#include <algorithm> // For std::max
#include <stdexcept> // For runtime_error in allocation

// Define STB implementations in exactly one .cpp file
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h" // For saving PNG

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"    // For font rendering

namespace { // Anonymous namespace for internal helpers

// --- Font Loading Helper ---
struct FontInfo {
    stbtt_fontinfo info;
    vector<unsigned char> buffer;
    bool loaded = false;
};

FontInfo loadFont(const string& fontPath) {
    FontInfo font;
    std::cout << "Loading font file: " << fontPath << " ..." << std::endl;
    font.buffer = readFileBytes(fontPath); // Use helper from common_types.h
    if (font.buffer.empty()) {
        std::cerr << "Error: Font file buffer is empty or could not be read: " << fontPath << std::endl;
        return font;
    }
    if (!stbtt_InitFont(&font.info, font.buffer.data(), stbtt_GetFontOffsetForIndex(font.buffer.data(), 0))) {
        std::cerr << "Error: Failed to initialize font: " << fontPath << std::endl;
        return font;
    }
    std::cout << "Font loaded successfully: " << fontPath << std::endl;
    font.loaded = true;
    return font;
}

// --- Rendering Helpers ---

struct RenderMetrics {
    int charWidthPx = 0;
    int lineHeightPx = 0;
    int outputImageWidthPx = 0;
    int outputImageHeightPx = 0;
    float scale = 0.0f;
    int ascentPx = 0;
    // int descentPx = 0; // Not directly used in render loop below
    // int lineGapPx = 0; // Not directly used in render loop below
    bool valid = false;
};

RenderMetrics calculateOutputDimensions(const stbtt_fontinfo& fontInfo, float fontSize, int asciiWidth, int asciiHeight) {
    RenderMetrics metrics;
    if (asciiWidth <= 0 || asciiHeight <= 0) {
         std::cerr << "Error: Invalid ASCII dimensions (" << asciiWidth << "x" << asciiHeight << ") for rendering." << std::endl;
         return metrics; // valid = false
    }


    metrics.scale = stbtt_ScaleForPixelHeight(&fontInfo, fontSize);
    if (metrics.scale <= 0) {
        std::cerr << "Error: Calculated font scale is invalid for font size " << fontSize << std::endl;
        return metrics;
    }

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
    metrics.ascentPx = static_cast<int>(std::round(ascent * metrics.scale));
    int descentPx = static_cast<int>(std::round(descent * metrics.scale)); // Local var needed for height calc
    int lineGapPx = static_cast<int>(std::round(lineGap * metrics.scale)); // Local var needed for height calc

    metrics.lineHeightPx = metrics.ascentPx - descentPx + lineGapPx;
    metrics.lineHeightPx = std::max(1, metrics.lineHeightPx); // Ensure positive line height

    // Use 'M' or a common wide character to estimate char width for monospaced look
    int advanceWidth, leftSideBearing;
    stbtt_GetCodepointHMetrics(&fontInfo, 'M', &advanceWidth, &leftSideBearing);
    metrics.charWidthPx = static_cast<int>(std::round(advanceWidth * metrics.scale));
    metrics.charWidthPx = std::max(1, metrics.charWidthPx); // Ensure positive char width


    metrics.outputImageWidthPx = asciiWidth * metrics.charWidthPx;
    metrics.outputImageHeightPx = asciiHeight * metrics.lineHeightPx;

    metrics.outputImageWidthPx = std::max(1, metrics.outputImageWidthPx);
    metrics.outputImageHeightPx = std::max(1, metrics.outputImageHeightPx);

    metrics.valid = true;
    return metrics;
}

void setSchemeColors(ColorScheme scheme, unsigned char bgColor[3], unsigned char fgColor[3]) {
    // (Copied directly from original code)
      switch (scheme) {
        case ColorScheme::AMBER_ON_BLACK:
            bgColor[0] = 0x00; bgColor[1] = 0x00; bgColor[2] = 0x00; fgColor[0] = 0xFF; fgColor[1] = 0xBF; fgColor[2] = 0x00; break;
        case ColorScheme::BLACK_ON_YELLOW:
            bgColor[0] = 0xFF; bgColor[1] = 0xFF; bgColor[2] = 0xAA; fgColor[0] = 0x00; fgColor[1] = 0x00; fgColor[2] = 0x00; break;
        case ColorScheme::BLACK_ON_CYAN:
            bgColor[0] = 0xAA; bgColor[1] = 0xFF; bgColor[2] = 0xFF; fgColor[0] = 0x00; fgColor[1] = 0x00; fgColor[2] = 0x00; break;
        case ColorScheme::COLOR_ON_WHITE:
            // Background slightly off-white, Foreground is derived from pixel
            bgColor[0] = 0xC8; bgColor[1] = 0xC8; bgColor[2] = 0xC8; fgColor[0] = 0; fgColor[1] = 0; fgColor[2] = 0; break;
        case ColorScheme::COLOR_ON_BLACK:
             // Background slightly off-black, Foreground is derived from pixel
            bgColor[0] = 0x36; bgColor[1] = 0x36; bgColor[2] = 0x36; fgColor[0] = 0; fgColor[1] = 0; fgColor[2] = 0; break;
        case ColorScheme::CYAN_ON_BLACK:
            bgColor[0] = 0x00; bgColor[1] = 0x00; bgColor[2] = 0x00; fgColor[0] = 0x00; fgColor[1] = 0xFF; fgColor[2] = 0xFF; break;
        case ColorScheme::GRAY_ON_BLACK:
             bgColor[0] = 0x00; bgColor[1] = 0x00; bgColor[2] = 0x00; fgColor[0] = 0xAA; fgColor[1] = 0xAA; fgColor[2] = 0xAA; break;
        case ColorScheme::GREEN_ON_BLACK:
            bgColor[0] = 0x00; bgColor[1] = 0x00; bgColor[2] = 0x00; fgColor[0] = 0x00; fgColor[1] = 0xFF; fgColor[2] = 0x00; break;
        case ColorScheme::MAGENTA_ON_BLACK:
            bgColor[0] = 0x00; bgColor[1] = 0x00; bgColor[2] = 0x00; fgColor[0] = 0xFF; fgColor[1] = 0x00; fgColor[2] = 0xFF; break;
        case ColorScheme::PURPLE_ON_BLACK:
            bgColor[0] = 0x00; bgColor[1] = 0x00; bgColor[2] = 0x00; fgColor[0] = 0x80; fgColor[1] = 0x00; fgColor[2] = 0x80; break;
        case ColorScheme::SEPIA:
            bgColor[0] = 0xF0; bgColor[1] = 0xE6; bgColor[2] = 0x8C; fgColor[0] = 0x70; fgColor[1] = 0x42; fgColor[2] = 0x14; break;
        case ColorScheme::SOLARIZED_DARK:
            bgColor[0] = 0x00; bgColor[1] = 0x2b; bgColor[2] = 0x36; fgColor[0] = 0x83; fgColor[1] = 0x94; fgColor[2] = 0x96; break;
        case ColorScheme::SOLARIZED_LIGHT:
            bgColor[0] = 0xfd; bgColor[1] = 0xf6; bgColor[2] = 0xe3; fgColor[0] = 0x65; fgColor[1] = 0x7b; fgColor[2] = 0x83; break;
        case ColorScheme::WHITE_ON_BLACK:
            bgColor[0] = 0x00; bgColor[1] = 0x00; bgColor[2] = 0x00; fgColor[0] = 0xFF; fgColor[1] = 0xFF; fgColor[2] = 0xFF; break;
        case ColorScheme::WHITE_ON_BLUE:
            bgColor[0] = 0x00; bgColor[1] = 0x00; bgColor[2] = 0xAA; fgColor[0] = 0xFF; fgColor[1] = 0xFF; fgColor[2] = 0xFF; break;
        case ColorScheme::WHITE_ON_DARK_RED:
            bgColor[0] = 0x8B; bgColor[1] = 0x00; bgColor[2] = 0x00; fgColor[0] = 0xFF; fgColor[1] = 0xFF; fgColor[2] = 0xFF; break;
        case ColorScheme::YELLOW_ON_BLACK:
             bgColor[0] = 0x00; bgColor[1] = 0x00; bgColor[2] = 0x00; fgColor[0] = 0xFF; fgColor[1] = 0xFF; fgColor[2] = 0x00; break;
        case ColorScheme::BLACK_ON_WHITE:
        default: // Default to Black on White (or slightly off-white)
            bgColor[0] = 0xC8; bgColor[1] = 0xC8; bgColor[2] = 0xC8; fgColor[0] = 0x00; fgColor[1] = 0x00; fgColor[2] = 0x00; break;
    }
}

// Renders a single character (glyph) onto the output image buffer
void renderGlyph(vector<unsigned char>& outputImageData,
                 const stbtt_fontinfo& fontInfo, float scale, char c,
                 int drawX_base, int drawY_base,
                 int imgWidth, int imgHeight,
                 const unsigned char finalColor[3], // Color to draw the glyph
                 const unsigned char bgColor[3])    // Background color for blending
{
    int char_w, char_h, xoff, yoff;
    unsigned char* bitmap = stbtt_GetCodepointBitmap(&fontInfo, scale, scale, c, &char_w, &char_h, &xoff, &yoff);

    if (!bitmap) return; // No bitmap for this character (e.g., space)

    for (int y = 0; y < char_h; ++y) {
        for (int x = 0; x < char_w; ++x) {
            int outX = drawX_base + xoff + x;
            int outY = drawY_base + yoff + y;

            // Check bounds
            if (outX >= 0 && outX < imgWidth && outY >= 0 && outY < imgHeight) {
                unsigned char alpha = bitmap[y * char_w + x];
                if (alpha > 10) { // Alpha threshold for drawing
                    size_t pixelIndex = (static_cast<size_t>(outY) * imgWidth + outX) * OUTPUT_CHANNELS;
                    float alphaF = alpha / 255.0f;

                    // Alpha blending: NewColor * alpha + OldColor * (1 - alpha)
                    outputImageData[pixelIndex]     = static_cast<unsigned char>(finalColor[0] * alphaF + bgColor[0] * (1.0f - alphaF));
                    outputImageData[pixelIndex + 1] = static_cast<unsigned char>(finalColor[1] * alphaF + bgColor[1] * (1.0f - alphaF));
                    outputImageData[pixelIndex + 2] = static_cast<unsigned char>(finalColor[2] * alphaF + bgColor[2] * (1.0f - alphaF));
                    // Note: The original code blended with whatever was already in the buffer.
                    // Blending with the specified bgColor ensures consistency if glyphs overlap slightly
                    // or if the background wasn't perfectly filled initially.
                }
            }
        }
    }
    stbtt_FreeBitmap(bitmap, nullptr); // Free the glyph bitmap
}


// Saves the image data buffer to a PNG file
bool saveImagePng(const path& outputPath, int width, int height, int channels, const vector<unsigned char>& data) {
    if (width <= 0 || height <= 0) {
        std::cerr << "Error: Invalid dimensions (" << width << "x" << height << ") for saveImage." << std::endl;
        return false;
    }
    size_t expectedSize = static_cast<size_t>(width) * height * channels;
     if (data.size() != expectedSize) {
         std::cerr << "Error: Data size (" << data.size() << ") != expected (" << expectedSize << ") for saveImage." << std::endl;
         // This often indicates an allocation issue or miscalculation earlier.
         return false;
     }

    if (!stbi_write_png(outputPath.string().c_str(), width, height, channels, data.data(), width * channels)) {
        std::cerr << "Error: Failed to save PNG image to '" << outputPath.string() << "'" << std::endl;
        return false;
    }
    return true;
}


} // end anonymous namespace

// --- Public Function Implementation ---

bool renderAsciiToImage(
    const vector<vector<CharColorInfo>>& asciiData,
    const std::filesystem::path& outputPath,
    const std::string& fontPath,
    float fontSize,
    ColorScheme scheme)
{
    if (asciiData.empty() || asciiData[0].empty()) {
        std::cerr << "Error: Cannot render empty ASCII data." << std::endl;
        return false;
    }

    // 1. Load Font
    FontInfo font = loadFont(fontPath);
    if (!font.loaded) {
        return false; // Error already printed by loadFont
    }

    // 2. Calculate Dimensions
    int asciiHeight = static_cast<int>(asciiData.size());
    int asciiWidth = static_cast<int>(asciiData[0].size());
    RenderMetrics metrics = calculateOutputDimensions(font.info, fontSize, asciiWidth, asciiHeight);

    if (!metrics.valid) {
        std::cerr << "Error: Could not calculate valid output dimensions for PNG." << std::endl;
        return false;
    }
     std::cout << "Calculated PNG output: " << metrics.outputImageWidthPx << "x" << metrics.outputImageHeightPx << std::endl;


    // 3. Determine Colors
    unsigned char bgColor[3], baseFgColor[3];
    setSchemeColors(scheme, bgColor, baseFgColor);
    bool usePixelColor = (scheme == ColorScheme::COLOR_ON_WHITE || scheme == ColorScheme::COLOR_ON_BLACK);

    // 4. Allocate Output Buffer
    vector<unsigned char> outputImageData;
    try {
        size_t required_size = static_cast<size_t>(metrics.outputImageWidthPx) * metrics.outputImageHeightPx * OUTPUT_CHANNELS;
        // Basic check against excessively large dimensions before allocation
        if (metrics.outputImageWidthPx <= 0 || metrics.outputImageHeightPx <= 0 || required_size == 0 ||
            static_cast<double>(metrics.outputImageWidthPx) * metrics.outputImageHeightPx > (10000.0 * 10000.0) ) { // Example sanity check limit
                 throw std::runtime_error("Calculated PNG dimensions are invalid or excessively large.");
        }
        outputImageData.resize(required_size);
    } catch (const std::bad_alloc& e) {
        std::cerr << "Error: Failed to allocate memory for PNG buffer (" << metrics.outputImageWidthPx << "x" << metrics.outputImageHeightPx << "): " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
         std::cerr << "Error: Allocating PNG buffer: " << e.what() << std::endl;
         return false;
    }


    // 5. Fill Background
    for (size_t i = 0; i < outputImageData.size(); i += OUTPUT_CHANNELS) {
        outputImageData[i]     = bgColor[0];
        outputImageData[i + 1] = bgColor[1];
        outputImageData[i + 2] = bgColor[2];
    }

    // 6. Render Characters (Glyphs)
    int currentY_baseline = metrics.ascentPx; // Start drawing baseline at first line's ascent

    for (const auto& lineData : asciiData) {
        int currentX = 0;
        for (const auto& charInfo : lineData) {
            char c = charInfo.character;

            // Determine color for this specific character
            const unsigned char* renderColor = usePixelColor ? charInfo.color : baseFgColor;

            // Render the glyph at the current position using calculated baseline and character origin
            renderGlyph(outputImageData, font.info, metrics.scale, c,
                        currentX, currentY_baseline, // Pass baseline Y
                        metrics.outputImageWidthPx, metrics.outputImageHeightPx,
                        renderColor, bgColor); // Pass background for blending

            // Advance X position using calculated character width (monospace assumption)
            // For variable width, would need stbtt_GetCodepointHMetrics here.
            currentX += metrics.charWidthPx;
        }
        // Move baseline down for the next line
        currentY_baseline += metrics.lineHeightPx;
    }

    // 7. Save Image
    std::cout << "Saving PNG: " << outputPath.filename().string() << std::endl;
    if (!saveImagePng(outputPath, metrics.outputImageWidthPx, metrics.outputImageHeightPx, OUTPUT_CHANNELS, outputImageData)) {
        return false; // Error already printed
    }

    return true;
}