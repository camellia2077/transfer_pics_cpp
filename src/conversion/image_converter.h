// image_converter.h
#ifndef IMAGE_CONVERTER_H
#define IMAGE_CONVERTER_H

#include "common_types.h" // Includes vector, string, CharColorInfo, path etc.
#include <filesystem>
#include <optional> // To return result or indicate error

struct AsciiConversionResult {
    vector<vector<CharColorInfo>> data;
    int originalWidth = 0;
    int originalHeight = 0;
    int asciiWidth = 0;
    int asciiHeight = 0;
};

// Converts an image file to its ASCII representation.
// Takes the image path and relevant config parameters.
// Returns an optional containing the result on success, or nullopt on failure.
std::optional<AsciiConversionResult> convertImageToAscii(
    const std::filesystem::path& imagePath,
    int targetAsciiWidth,
    double aspectRatioCorrection
);

#endif // IMAGE_CONVERTER_H