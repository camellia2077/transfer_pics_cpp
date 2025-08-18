// common_types.h
#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H
//定义了整个程序共享的基础数据类型、常量、枚举以及一些通用的辅助函数声明。
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <fstream>
#include <filesystem> // For path
#include <algorithm> // For std::transform
#include <cctype>    // For std::tolower
#include <iostream>  // For cerr in helpers

using std::string;
using std::vector;
using std::set;
using std::filesystem::path;

// --- Constants ---
const string ASCII_CHARS = "@%#*+=-:. ";
const int NUM_ASCII_CHARS = static_cast<int>(ASCII_CHARS.length());
const int OUTPUT_CHANNELS = 3; // Output PNG as RGB

const set<string> SUPPORTED_EXTENSIONS = {
    ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif"
};

// --- Enums ---
enum class ColorScheme {
    AMBER_ON_BLACK, BLACK_ON_YELLOW, BLACK_ON_CYAN, COLOR_ON_WHITE,
    COLOR_ON_BLACK, CYAN_ON_BLACK, GRAY_ON_BLACK, GREEN_ON_BLACK,
    MAGENTA_ON_BLACK, PURPLE_ON_BLACK, SEPIA, SOLARIZED_DARK,
    SOLARIZED_LIGHT, WHITE_ON_BLACK, WHITE_ON_BLUE, WHITE_ON_DARK_RED,
    YELLOW_ON_BLACK, BLACK_ON_WHITE,
};

// --- Structures ---
struct CharColorInfo {
    char character;
    unsigned char color[3]; // Original color from the image
};

struct Config {
    int targetWidth = 1024;
    double charAspectRatioCorrection = 2.0;
    string fontFilename = "Consolas.ttf"; // Relative name from config
    float fontSize = 15.0f;              // Font size for PNG
    string finalFontPath = "";           // Resolved absolute/relative path used
    bool enableTiledRendering = false;   // Note: Tiling logic wasn't in the original rendering part
    int tileSize = 512;
    string imageOutputSubDirSuffix = "_ascii_output";
    string batchOutputSubDirSuffix = "_ascii_batch_output";

    vector<ColorScheme> schemesToGenerate = {
        ColorScheme::BLACK_ON_WHITE,
        ColorScheme::COLOR_ON_WHITE,
    };

    // HTML Output Settings
    bool generateHtmlOutput = true;
    float htmlFontSizePt = 8.0f;         // Font size for HTML output in points
};

// --- Helper Functions (moved here for common use) ---

inline string toLower(string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return s;
}

// Map from lowercase string name to ColorScheme enum
const std::unordered_map<std::string, ColorScheme>& getColorSchemeMap(); // Declare, define in config_handler.cpp

// Helper function to convert ColorScheme enum to string (for printing/filenames)
string colorSchemeToString(ColorScheme scheme); // Declare, define in config_handler.cpp
string getSchemeSuffix(ColorScheme scheme);     // Declare, define in config_handler.cpp

inline bool isImageFile(const path& p) {
    if (!p.has_extension()) return false;
    string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return SUPPORTED_EXTENSIONS.count(ext);
}

// Helper to read a file into a byte vector
inline vector<unsigned char> readFileBytes(const string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Error: Cannot open file '" << filename << "'" << std::endl;
        return {};
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    vector<unsigned char> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cerr << "Error: Cannot read file '" << filename << "'" << std::endl;
        return {};
    }
    return buffer;
}


#endif // COMMON_TYPES_H