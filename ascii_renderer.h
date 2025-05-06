// ascii_renderer.h
#ifndef ASCII_RENDERER_H
#define ASCII_RENDERER_H

#include "common_types.h" // Includes vector, string, path, CharColorInfo, ColorScheme etc.
#include <filesystem>

// Renders the ASCII data to a PNG image file.
// Takes the ASCII data, output path, font path, font size, and color scheme.
// Returns true on success, false on failure.
bool renderAsciiToImage(
    const vector<vector<CharColorInfo>>& asciiData,
    const std::filesystem::path& outputPath,
    const std::string& fontPath, // Use the resolved finalFontPath from Config
    float fontSize,
    ColorScheme scheme
);


#endif // ASCII_RENDERER_H