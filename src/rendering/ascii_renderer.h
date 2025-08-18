// ascii_renderer.h
#ifndef ASCII_RENDERER_H
#define ASCII_RENDERER_H

#include "common_types.h" // Includes vector, string, path, CharColorInfo, ColorScheme etc.
#include <filesystem>

// Renders the ASCII data to a PNG image file.
bool renderAsciiToImage(
    const vector<vector<CharColorInfo>>& asciiData,
    const std::filesystem::path& outputPath,
    const std::string& fontPath, // Use the resolved finalFontPath from Config
    float fontSize,              // Font size for PNG from Config
    ColorScheme scheme
);

// Renders the ASCII data to an HTML file.
bool renderAsciiToHtml(
    const vector<vector<CharColorInfo>>& asciiData,
    const std::filesystem::path& outputPath,
    const Config& config,     // Pass full config for font name, HTML font size, etc.
    ColorScheme scheme        // The specific color scheme to apply
);

#endif // ASCII_RENDERER_H