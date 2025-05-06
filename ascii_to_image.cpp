// ascii_to_image.cpp
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream>      // For file operations
#include <memory>       // For unique_ptr
#include <filesystem>   // For path operations (C++17)
#include <system_error> // For filesystem errors
#include <chrono>       // For timing
#include <iomanip>      // For setprecision
#include <sstream>      // For stringstream <--- Added
#include <cctype>       // For tolower
#include <set>          // For supported extensions
#include <future>       // For std::async, std::future
#include <thread>       // For std::thread::hardware_concurrency
#include <functional>   // For std::cref
#include <unordered_map>// For schemeNameMap <--- Added

// ADD THIS BLOCK FOR WINDOWS API
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif
// Define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // Needs to be in your include path or project

// Define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h" // Needs to be in your include path or project

// Define STB_TRUETYPE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h" // Needs to be in your include path or project


#include <ini.h>

using namespace std;
using std::filesystem::path;
using namespace std::chrono;

// --- Common Constants, Types, and Structures ---
// ... (ASCII_CHARS, NUM_ASCII_CHARS, OUTPUT_CHANNELS, SUPPORTED_EXTENSIONS remain the same) ...
const string ASCII_CHARS = "@%#*+=-:. ";
const int NUM_ASCII_CHARS = static_cast<int>(ASCII_CHARS.length());
const int OUTPUT_CHANNELS = 3; // Output PNG as RGB

const set<string> SUPPORTED_EXTENSIONS = {
    ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif"
};


enum class ColorScheme {
    AMBER_ON_BLACK, BLACK_ON_YELLOW, BLACK_ON_CYAN, COLOR_ON_WHITE,
    COLOR_ON_BLACK, CYAN_ON_BLACK, GRAY_ON_BLACK, GREEN_ON_BLACK,
    MAGENTA_ON_BLACK, PURPLE_ON_BLACK, SEPIA, SOLARIZED_DARK,
    SOLARIZED_LIGHT, WHITE_ON_BLACK, WHITE_ON_BLUE, WHITE_ON_DARK_RED,
    YELLOW_ON_BLACK, BLACK_ON_WHITE,
};

// Helper function to convert ColorScheme enum to string (for printing)
string colorSchemeToString(ColorScheme scheme) {
    switch (scheme) {
        case ColorScheme::AMBER_ON_BLACK: return "AmberOnBlack";
        case ColorScheme::BLACK_ON_YELLOW: return "BlackOnYellow";
        case ColorScheme::BLACK_ON_CYAN: return "BlackOnCyan";
        case ColorScheme::COLOR_ON_WHITE: return "ColorOnWhite";
        case ColorScheme::COLOR_ON_BLACK: return "ColorOnBlack";
        case ColorScheme::CYAN_ON_BLACK: return "CyanOnBlack";
        case ColorScheme::GRAY_ON_BLACK: return "GrayOnBlack";
        case ColorScheme::GREEN_ON_BLACK: return "GreenOnBlack";
        case ColorScheme::MAGENTA_ON_BLACK: return "MagentaOnBlack";
        case ColorScheme::PURPLE_ON_BLACK: return "PurpleOnBlack";
        case ColorScheme::SEPIA: return "Sepia";
        case ColorScheme::SOLARIZED_DARK: return "SolarizedDark";
        case ColorScheme::SOLARIZED_LIGHT: return "SolarizedLight";
        case ColorScheme::WHITE_ON_BLACK: return "WhiteOnBlack";
        case ColorScheme::WHITE_ON_BLUE: return "WhiteOnBlue";
        case ColorScheme::WHITE_ON_DARK_RED: return "WhiteOnDarkRed";
        case ColorScheme::YELLOW_ON_BLACK: return "YellowOnBlack";
        case ColorScheme::BLACK_ON_WHITE: return "BlackOnWhite";
        default: return "UnknownScheme";
    }
}

// Helper to convert string to lowercase
std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return s;
}

// Map from lowercase string name to ColorScheme enum
// Defined globally (or static) so iniHandler can access it
const std::unordered_map<std::string, ColorScheme> schemeNameMap = {
    {"amberonblack", ColorScheme::AMBER_ON_BLACK},
    {"blackonyellow", ColorScheme::BLACK_ON_YELLOW},
    {"blackoncyan", ColorScheme::BLACK_ON_CYAN},
    {"coloronwhite", ColorScheme::COLOR_ON_WHITE},
    {"coloronblack", ColorScheme::COLOR_ON_BLACK},
    {"cyanonblack", ColorScheme::CYAN_ON_BLACK},
    {"grayonblack", ColorScheme::GRAY_ON_BLACK},
    {"greenonblack", ColorScheme::GREEN_ON_BLACK},
    {"magentaonblack", ColorScheme::MAGENTA_ON_BLACK},
    {"purpleonblack", ColorScheme::PURPLE_ON_BLACK},
    {"sepia", ColorScheme::SEPIA},
    {"solarizeddark", ColorScheme::SOLARIZED_DARK},
    {"solarizedlight", ColorScheme::SOLARIZED_LIGHT},
    {"whiteonblack", ColorScheme::WHITE_ON_BLACK},
    {"whiteonblue", ColorScheme::WHITE_ON_BLUE},
    {"whiteondarkred", ColorScheme::WHITE_ON_DARK_RED},
    {"yellowonblack", ColorScheme::YELLOW_ON_BLACK},
    {"blackonwhite", ColorScheme::BLACK_ON_WHITE}
};


struct CharColorInfo {
    char character;
    unsigned char color[3];
};

struct Config {
    int targetWidth = 1024;
    double charAspectRatioCorrection = 2.0;
    string fontFilename = "Consolas.ttf";
    float fontSize = 15.0f; // PNG font size
    // float pdfFontSize = 10.0f; // Not used by this program directly
    string finalFontPath = "";
    bool enableTiledRendering = false;
    int tileSize = 512;
    string baseOutputFilename = "output_ascii_art"; // Not directly used, image name is base
    string outputPngExtension = ".png";
    // string outputPdfExtension = ".pdf"; // Not used
    string imageOutputSubDirSuffix = "_ascii_output";
    string batchOutputSubDirSuffix = "_ascii_batch_output";

    // Default schemes - will be overridden by INI if colorSchemes key is found
    vector<ColorScheme> schemesToGenerate = {
        ColorScheme::BLACK_ON_WHITE,
        ColorScheme::COLOR_ON_WHITE,
    };
};

// --- Common INI Handler ---
static int iniHandler(void* user, const char* section, const char* name, const char* value_cstr) {
    Config* pconfig = static_cast<Config*>(user);
    string value(value_cstr);

    if (strcmp(section, "Settings") != 0) {
        return 1;
    }

    #define LOG_LOADED(key, val) cout << "  -> Loaded " #key " = " << val << endl

    try {
        if (strcmp(name, "targetWidth") == 0) {
            pconfig->targetWidth = stoi(value);
            LOG_LOADED(targetWidth, pconfig->targetWidth);
        } else if (strcmp(name, "charAspectRatioCorrection") == 0) {
            pconfig->charAspectRatioCorrection = stod(value);
            LOG_LOADED(charAspectRatioCorrection, pconfig->charAspectRatioCorrection);
        } else if (strcmp(name, "fontFilename") == 0) {
            if (!value.empty()) {
                pconfig->fontFilename = value;
                LOG_LOADED(fontFilename, pconfig->fontFilename);
            } else {
                cerr << "Warning: Empty value for 'fontFilename'. Using default." << endl;
            }
        } else if (strcmp(name, "fontSize") == 0) {
            pconfig->fontSize = stof(value);
            LOG_LOADED(fontSize (PNG), pconfig->fontSize);
        } else if (strcmp(name, "enableTiledRendering") == 0) {
            string lower_val = value;
            transform(lower_val.begin(), lower_val.end(), lower_val.begin(), ::tolower);
            if (lower_val == "true" || lower_val == "yes" || lower_val == "1") pconfig->enableTiledRendering = true;
            else if (lower_val == "false" || lower_val == "no" || lower_val == "0") pconfig->enableTiledRendering = false;
            else {
                cerr << "Warning: Invalid boolean for 'enableTiledRendering': \"" << value << "\". Using default." << endl;
                return 0;
            }
            LOG_LOADED(enableTiledRendering, pconfig->enableTiledRendering);
        } else if (strcmp(name, "tileSize") == 0) {
            pconfig->tileSize = stoi(value);
            if (pconfig->tileSize <= 0) {
                cerr << "Warning: tileSize must be positive. Using default 512." << endl;
                pconfig->tileSize = 512;
            }
            LOG_LOADED(tileSize, pconfig->tileSize);
        }
        // --- NEW: Handle colorSchemes ---
        else if (strcmp(name, "colorSchemes") == 0) {
            if (!value.empty()) {
                pconfig->schemesToGenerate.clear(); // Clear defaults first
                std::stringstream ss(value);
                std::string schemeNameStr;
                int loadedCount = 0;
                string loadedSchemeNamesStr; // For logging

                while (std::getline(ss, schemeNameStr, ',')) { // Split by comma
                    // Trim whitespace from schemeNameStr
                    size_t first = schemeNameStr.find_first_not_of(" \t\n\r\f\v");
                    if (string::npos == first) continue; // Skip empty segments
                    size_t last = schemeNameStr.find_last_not_of(" \t\n\r\f\v");
                    schemeNameStr = schemeNameStr.substr(first, (last - first + 1));

                    if (!schemeNameStr.empty()) {
                        std::string lowerSchemeName = toLower(schemeNameStr);
                        auto it = schemeNameMap.find(lowerSchemeName);
                        if (it != schemeNameMap.end()) {
                            pconfig->schemesToGenerate.push_back(it->second);
                            if (loadedCount > 0) loadedSchemeNamesStr += ", ";
                            loadedSchemeNamesStr += schemeNameStr; // Log original case name
                            loadedCount++;
                        } else {
                            cerr << "Warning: Unknown color scheme name in config: '" << schemeNameStr << "'. Ignoring." << endl;
                        }
                    }
                }

                if (loadedCount > 0) {
                     cout << "  -> Loaded colorSchemes = " << loadedSchemeNamesStr << endl;
                } else {
                     // If the INI string was present but contained no valid schemes, use defaults.
                     cerr << "Warning: No valid color schemes found for 'colorSchemes' key. Using defaults." << endl;
                     // Re-add defaults explicitly or just don't clear them initially if key exists but value is empty/invalid.
                     // Let's stick to the original defaults if parsing fails or yields nothing.
                     // So, we only clear IF we successfully load at least one.
                     // If loadedCount is 0 here, schemesToGenerate might be empty, revert to default.
                     if (pconfig->schemesToGenerate.empty()) {
                         pconfig->schemesToGenerate = { ColorScheme::BLACK_ON_WHITE, ColorScheme::COLOR_ON_WHITE };
                         cout << "  -> Reverted to default schemes." << endl;
                     }
                }
            } else {
                cerr << "Warning: Empty value for 'colorSchemes' in config. Using default schemes." << endl;
                // Keep the hardcoded defaults if the value is empty
            }
        }
        // --- End Handle colorSchemes ---

    } catch (const std::invalid_argument& e) {
        cerr << "Warning: Invalid data format for key '" << name << "' in config.ini. Value: \"" << value << "\". Error: " << e.what() << endl;
        return 0; // Return 0 to indicate error during value parsing
    } catch (const std::out_of_range& e) {
        cerr << "Warning: Value out of range for key '" << name << "' in config.ini. Value: \"" << value << "\". Error: " << e.what() << endl;
        return 0; // Return 0 to indicate error during value parsing
    }
    return 1; // Return 1 on successful handling of the key-value pair
}


// --- Common Utility Functions ---
// ... (trim, isImageFile, read_file, getSchemeSuffix, loadConfiguration, getUserInputPath, setupImageOutputSubdirectory, loadImage, generateAsciiData, loadFontInfo, calculateOutputDimensions, setSchemeColors remain the same) ...
// NOTE: getSchemeSuffix is still useful for generating filenames based on the enum
string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\n\r\f\v");
    if (string::npos == first) return str;
    size_t last = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(first, (last - first + 1));
}

bool isImageFile(const path& p) {
    if (!p.has_extension()) return false;
    string ext = p.extension().string();
    transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return SUPPORTED_EXTENSIONS.count(ext);
}

vector<unsigned char> read_file(const string& filename) {
    ifstream file(filename, ios::binary | ios::ate);
    if (!file) {
        cerr << "Error: Cannot open file '" << filename << "'" << endl;
        return {};
    }
    streamsize size = file.tellg();
    file.seekg(0, ios::beg);
    vector<unsigned char> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        cerr << "Error: Cannot read file '" << filename << "'" << endl;
        return {};
    }
    return buffer;
}

string getSchemeSuffix(ColorScheme scheme) {
    switch (scheme) {
        case ColorScheme::BLACK_ON_WHITE:   return "_BlackOnWhite";
        case ColorScheme::WHITE_ON_BLACK:   return "_WhiteOnBlack";
        case ColorScheme::GREEN_ON_BLACK:   return "_GreenOnBlack";
        case ColorScheme::PURPLE_ON_BLACK:  return "_PurpleOnBlack";
        case ColorScheme::COLOR_ON_WHITE:   return "_ColorOnWhite";
        case ColorScheme::COLOR_ON_BLACK:   return "_ColorOnBlack";
        case ColorScheme::AMBER_ON_BLACK:   return "_AmberOnBlack";
        case ColorScheme::CYAN_ON_BLACK:    return "_CyanOnBlack";
        case ColorScheme::YELLOW_ON_BLACK:  return "_YellowOnBlack";
        case ColorScheme::WHITE_ON_BLUE:    return "_WhiteOnBlue";
        case ColorScheme::BLACK_ON_YELLOW:  return "_BlackOnYellow";
        case ColorScheme::BLACK_ON_CYAN:    return "_BlackOnCyan";
        case ColorScheme::SEPIA:            return "_Sepia";
        case ColorScheme::MAGENTA_ON_BLACK: return "_MagentaOnBlack";
        case ColorScheme::WHITE_ON_DARK_RED:return "_WhiteOnDarkRed";
        case ColorScheme::GRAY_ON_BLACK:    return "_GrayOnBlack";
        case ColorScheme::SOLARIZED_DARK:   return "_SolarizedDark";
        case ColorScheme::SOLARIZED_LIGHT:  return "_SolarizedLight";
        default:                            return "_UnknownScheme";
    }
}

bool loadConfiguration(const path& configPath, Config& config) {
    // Make sure the default schemes are set before parsing, in case the key is missing
    // or parsing fails partially. The handler will clear/overwrite if the key is found.
    config.schemesToGenerate = { ColorScheme::BLACK_ON_WHITE, ColorScheme::COLOR_ON_WHITE };

    cout << "Info: Attempting to load configuration from '" << configPath.string() << "'..." << endl;
    int parseResult = ini_parse(configPath.string().c_str(), iniHandler, &config);
    if (parseResult < 0) {
        cout << "Info: Config file '" << configPath.string() << "' not found. Using defaults." << endl;
        // Defaults are already set, so just return true
        return true;
    } else if (parseResult > 0) {
        cerr << "Warning: Error parsing config '" << configPath.string() << "' at line " << parseResult << "." << endl;
        cerr << "Warning: Check format/values. Settings before error and defaults for others will be used." << endl;
         // Even with parsing errors, we might have loaded *some* values.
         // Check if schemes ended up empty due to error after clearing
         if (config.schemesToGenerate.empty()) {
             cerr << "Warning: Reverting to default color schemes due to parsing issue." << endl;
             config.schemesToGenerate = { ColorScheme::BLACK_ON_WHITE, ColorScheme::COLOR_ON_WHITE };
         }
        return false; // Indicate warning/error occurred
    }

    // If parseResult is 0, loading was successful. Check if INI loading left schemes empty.
     if (config.schemesToGenerate.empty()) {
         cerr << "Warning: No color schemes specified or loaded from config. Using defaults." << endl;
         config.schemesToGenerate = { ColorScheme::BLACK_ON_WHITE, ColorScheme::COLOR_ON_WHITE };
     }

    cout << "Info: Configuration loaded successfully." << endl;
    return true;
}

string getUserInputPath() {
    string inputPathStr;
    cout << "Please enter the image file or folder path for IMAGE generation: ";
    getline(cin, inputPathStr);
    if (inputPathStr.empty()) {
        cerr << "Error: Input path is empty." << endl;
    }
    return inputPathStr;
}

path setupImageOutputSubdirectory(const path& baseOutputDir, const path& imagePath, const string& suffix) {
    string subDirName = imagePath.stem().string() + suffix;
    path outputSubDirPath = baseOutputDir / subDirName;
    try {
        if (filesystem::create_directories(outputSubDirPath)) {
            // cout << "Created output subdirectory: " << outputSubDirPath.string() << endl;
        } else if (!filesystem::exists(outputSubDirPath) || !filesystem::is_directory(outputSubDirPath)) {
             cerr << "Error: Failed to create or access output subdirectory: " << outputSubDirPath.string() << endl;
             return path();
        }
        return outputSubDirPath;
    } catch (const filesystem::filesystem_error& e) {
        cerr << "Error (filesystem): Creating subdirectory " << outputSubDirPath.string() << ": " << e.what() << endl;
        return path();
    }
}

unique_ptr<unsigned char, void(*)(void*)> loadImage(const string& imagePath, int& width, int& height) {
    unsigned char *data = stbi_load(imagePath.c_str(), &width, &height, nullptr, OUTPUT_CHANNELS);
    if (data == nullptr) {
        cerr << "Error: Failed to load image '" << imagePath << "'. Reason: " << stbi_failure_reason() << endl;
        return unique_ptr<unsigned char, void(*)(void*)>(nullptr, stbi_image_free);
    }
    return unique_ptr<unsigned char, void(*)(void*)>(data, stbi_image_free);
}

vector<vector<CharColorInfo>> generateAsciiData(const unsigned char* imgData, int width, int height, int targetWidth, int targetHeight) {
    vector<vector<CharColorInfo>> asciiResultData;
    if (!imgData || width <= 0 || height <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        cerr << "Error: Invalid arguments to generateAsciiData." << endl;
        return asciiResultData;
    }
    asciiResultData.reserve(targetHeight);
    double xScale = static_cast<double>(width) / targetWidth;
    double yScale = static_cast<double>(height) / targetHeight;

    for (int yOut = 0; yOut < targetHeight; ++yOut) {
        vector<CharColorInfo> currentLineData;
        currentLineData.reserve(targetWidth);
        for (int xOut = 0; xOut < targetWidth; ++xOut) {
            int xImg = static_cast<int>(std::floor((xOut + 0.5) * xScale));
            int yImg = static_cast<int>(std::floor((yOut + 0.5) * yScale));
            xImg = max(0, min(xImg, width - 1));
            yImg = max(0, min(yImg, height - 1));

            size_t pixelOffset = (static_cast<size_t>(yImg) * width + xImg) * OUTPUT_CHANNELS;
            unsigned char r = imgData[pixelOffset];
            unsigned char g = imgData[pixelOffset + 1];
            unsigned char b = imgData[pixelOffset + 2];
            int gray = (static_cast<int>(r) + g + b) / 3;
            int asciiIndex = static_cast<int>(std::floor((gray / 255.0f) * (NUM_ASCII_CHARS -1) )); // ensure index is valid
            asciiIndex = max(0, min(asciiIndex, NUM_ASCII_CHARS - 1));


            CharColorInfo info;
            info.character = ASCII_CHARS[static_cast<size_t>(asciiIndex)];
            info.color[0] = r; info.color[1] = g; info.color[2] = b;
            currentLineData.push_back(info);
        }
        asciiResultData.push_back(currentLineData);
    }
    return asciiResultData;
}

bool loadFontInfo(const string& fontPath, stbtt_fontinfo& fontInfo, vector<unsigned char>& fontBuffer) {
    cout << "\nLoading font file: " << fontPath << " ..." << endl;
    fontBuffer = read_file(fontPath);
    if (fontBuffer.empty()) return false;
    if (!stbtt_InitFont(&fontInfo, fontBuffer.data(), stbtt_GetFontOffsetForIndex(fontBuffer.data(), 0))) {
        cerr << "Error: Failed to initialize font." << endl;
        return false;
    }
    // Font name retrieval (optional, for info)
    string fontName = "Name not found";
    // ... (font name retrieval logic as in original) ...
    cout << "Font loaded successfully. Location: " << fontPath << endl; // Name: " << fontName << endl;
    return true;
}

void calculateOutputDimensions(const stbtt_fontinfo& fontInfo, float pngFontSize, int asciiTargetWidth, int asciiTargetHeight,
                               int& charWidthPx, int& lineHeightPx, int& outputImageWidthPx, int& outputImageHeightPx,
                               float& scaleForPng, int& ascentPx, int& descentPx, int& lineGapPx) {
    scaleForPng = stbtt_ScaleForPixelHeight(&fontInfo, pngFontSize);
    stbtt_GetFontVMetrics(&fontInfo, &ascentPx, &descentPx, &lineGapPx);
    ascentPx = static_cast<int>(std::round(ascentPx * scaleForPng));
    descentPx = static_cast<int>(std::round(descentPx * scaleForPng));
    lineGapPx = static_cast<int>(std::round(lineGapPx * scaleForPng));
    lineHeightPx = ascentPx - descentPx + lineGapPx;

    int advanceWidth;
    stbtt_GetCodepointHMetrics(&fontInfo, 'M', &advanceWidth, nullptr);
    charWidthPx = static_cast<int>(std::round(advanceWidth * scaleForPng));
    if (charWidthPx <= 0) charWidthPx = 1;
    if (lineHeightPx <= 0) lineHeightPx = 1;

    outputImageWidthPx = asciiTargetWidth * charWidthPx;
    outputImageHeightPx = asciiTargetHeight * lineHeightPx;
    outputImageWidthPx = max(1, outputImageWidthPx);
    outputImageHeightPx = max(1, outputImageHeightPx);
}

void setSchemeColors(ColorScheme scheme, unsigned char bgColor[3], unsigned char fgColor[3]) {
    // ... (same as original setSchemeColors function) ...
     switch (scheme) {
        case ColorScheme::AMBER_ON_BLACK:
            bgColor[0] = 0x00; bgColor[1] = 0x00; bgColor[2] = 0x00; fgColor[0] = 0xFF; fgColor[1] = 0xBF; fgColor[2] = 0x00; break;
        case ColorScheme::BLACK_ON_YELLOW:
            bgColor[0] = 0xFF; bgColor[1] = 0xFF; bgColor[2] = 0xAA; fgColor[0] = 0x00; fgColor[1] = 0x00; fgColor[2] = 0x00; break;
        case ColorScheme::BLACK_ON_CYAN:
            bgColor[0] = 0xAA; bgColor[1] = 0xFF; bgColor[2] = 0xFF; fgColor[0] = 0x00; fgColor[1] = 0x00; fgColor[2] = 0x00; break;
        case ColorScheme::COLOR_ON_WHITE:
            bgColor[0] = 0xC8; bgColor[1] = 0xC8; bgColor[2] = 0xC8; fgColor[0] = 0; fgColor[1] = 0; fgColor[2] = 0; break;
        case ColorScheme::COLOR_ON_BLACK:
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
        default:
            bgColor[0] = 0xC8; bgColor[1] = 0xC8; bgColor[2] = 0xC8; fgColor[0] = 0x00; fgColor[1] = 0x00; fgColor[2] = 0x00; break;
    }
}


// --- Image Program Specific Functions ---
void renderAsciiArtToImage(vector<unsigned char>& outputImageData,
                           const vector<vector<CharColorInfo>>& asciiData,
                           const stbtt_fontinfo& fontInfo,
                           ColorScheme scheme,
                           const unsigned char bgColor[3],
                           const unsigned char fgColor[3], // Fixed FG for non-color schemes
                           int imgWidth, int imgHeight,
                           int ascentPx, float scaleForPng, int lineHeightPx) {
    // Fill background
    for (size_t i = 0; i < outputImageData.size(); i += OUTPUT_CHANNELS) {
        outputImageData[i]     = bgColor[0];
        outputImageData[i + 1] = bgColor[1];
        outputImageData[i + 2] = bgColor[2];
    }

    int currentY = ascentPx;
    for (const auto& lineData : asciiData) {
        int currentX = 0;
        for (const auto& charInfo : lineData) {
            char c = charInfo.character;
            int char_w, char_h, xoff, yoff;
            unsigned char* bitmap = stbtt_GetCodepointBitmap(&fontInfo, scaleForPng, scaleForPng, c, &char_w, &char_h, &xoff, &yoff);

            int advanceWidthPx, leftSideBearingPx;
            stbtt_GetCodepointHMetrics(&fontInfo, c, &advanceWidthPx, &leftSideBearingPx);
            int charAdvancePx = static_cast<int>(std::round(advanceWidthPx * scaleForPng));

            if (bitmap) {
                int drawX_base = currentX + static_cast<int>(std::round(leftSideBearingPx * scaleForPng)) + xoff;
                int drawY_base = currentY + yoff;

                for (int y = 0; y < char_h; ++y) {
                    for (int x = 0; x < char_w; ++x) {
                        int outX = drawX_base + x;
                        int outY = drawY_base + y;
                        if (outX >= 0 && outX < imgWidth && outY >= 0 && outY < imgHeight) {
                            unsigned char alpha = bitmap[y * char_w + x];
                            if (alpha > 10) { // Alpha threshold
                                size_t pixelIndex = (static_cast<size_t>(outY) * imgWidth + outX) * OUTPUT_CHANNELS;
                                const unsigned char* finalColor = (scheme == ColorScheme::COLOR_ON_WHITE || scheme == ColorScheme::COLOR_ON_BLACK) ? charInfo.color : fgColor;
                                float alphaF = alpha / 255.0f;
                                outputImageData[pixelIndex]     = static_cast<unsigned char>(finalColor[0] * alphaF + outputImageData[pixelIndex] * (1.0f - alphaF));
                                outputImageData[pixelIndex + 1] = static_cast<unsigned char>(finalColor[1] * alphaF + outputImageData[pixelIndex + 1] * (1.0f - alphaF));
                                outputImageData[pixelIndex + 2] = static_cast<unsigned char>(finalColor[2] * alphaF + outputImageData[pixelIndex + 2] * (1.0f - alphaF));
                            }
                        }
                    }
                }
                stbtt_FreeBitmap(bitmap, nullptr);
            }
            currentX += charAdvancePx;
        }
        currentY += lineHeightPx;
    }
}

bool saveImagePng(const path& outputPath, int width, int height, int channels, const vector<unsigned char>& data) {
    size_t expectedSize = static_cast<size_t>(width) * height * channels;
    if (data.size() != expectedSize) {
        cerr << "Error: Data size (" << data.size() << ") != expected (" << expectedSize << ") for saveImage." << endl;
        return false;
    }
    if (width <= 0 || height <= 0) {
        cerr << "Error: Invalid dimensions (" << width << "x" << height << ") for saveImage." << endl;
        return false;
    }
    if (!stbi_write_png(outputPath.string().c_str(), width, height, channels, data.data(), width * channels)) {
        cerr << "Error: Failed to save PNG image to '" << outputPath.string() << "'" << endl;
        return false;
    }
    return true;
}


bool processImageFileForPng(const path& imagePath,
                            const path& baseOutputDir,
                            const Config& config,
                            const stbtt_fontinfo& fontInfo,
                            const vector<unsigned char>& fontBuffer) // fontBuffer not strictly needed here if fontInfo is enough
{
    cout << "\n==================================================" << endl;
    cout << "Processing IMAGE: " << imagePath.string() << endl;
    cout << "==================================================" << endl;

    string imageSubDirSuffixWithWidth = "_" + std::to_string(config.targetWidth) + config.imageOutputSubDirSuffix;
    path outputSubDirPath = setupImageOutputSubdirectory(baseOutputDir, imagePath, imageSubDirSuffixWithWidth);
    if (outputSubDirPath.empty()) {
        cerr << "Skipping image " << imagePath.filename().string() << " (output dir failure)." << endl;
        return false;
    }

    cout << "Loading image " << imagePath.filename().string() << "..." << endl;
    auto load_start = high_resolution_clock::now();
    int width, height;
    auto imgDataPtr = loadImage(imagePath.string(), width, height);
    auto load_end = high_resolution_clock::now();
    cout << "-> Loaded (" << width << "x" << height << "), Time: "
         << fixed << setprecision(3) << duration_cast<milliseconds>(load_end - load_start).count() / 1000.0 << "s" << endl;
    if (!imgDataPtr) return false;

    auto proc_start = high_resolution_clock::now();
    cout << "Generating ASCII data..." << endl;
    int targetHeightNum = static_cast<int>(std::round(static_cast<double>(height * config.targetWidth) / (width * config.charAspectRatioCorrection)));
    targetHeightNum = max(1, targetHeightNum);
    cout << "Calculated ASCII grid: " << config.targetWidth << "x" << targetHeightNum << endl;

    vector<vector<CharColorInfo>> asciiResultData = generateAsciiData(
        imgDataPtr.get(), width, height, config.targetWidth, targetHeightNum);
    if (asciiResultData.empty() || asciiResultData[0].empty()) {
        cerr << "Error: Failed to generate ASCII data for " << imagePath.filename().string() << ". Skipping." << endl;
        return false;
    }

    int charWidthPx, lineHeightPx, outputImageWidthPx, outputImageHeightPx, ascentPx, descentPx, lineGapPx;
    float scaleForPng;
    calculateOutputDimensions(fontInfo, config.fontSize, config.targetWidth, targetHeightNum,
                              charWidthPx, lineHeightPx, outputImageWidthPx, outputImageHeightPx,
                              scaleForPng, ascentPx, descentPx, lineGapPx);
    if (outputImageWidthPx <= 0 || outputImageHeightPx <= 0) {
        cerr << "Error: Invalid PNG output dimensions for " << imagePath.filename().string() << ". Skipping." << endl;
        return false;
    }
    cout << "Calculated PNG output: " << outputImageWidthPx << "x" << outputImageHeightPx << endl;

    bool allSchemesSuccessful = true;
    // Use the schemes loaded into the config struct
    if (config.schemesToGenerate.empty()) {
         cerr << "Error: No color schemes configured to generate for " << imagePath.filename().string() << ". Skipping rendering." << endl;
         return false; // Or maybe true if loading/ASCII gen was okay? Depends on desired behaviour. Let's say false.
    }
    cout << "Processing " << config.schemesToGenerate.size() << " configured color scheme(s)..." << endl; // Log how many schemes are being processed

    for (const auto& currentScheme : config.schemesToGenerate) {
        string schemeSuffix = getSchemeSuffix(currentScheme); // Use existing function
        string baseNameForOutput = imagePath.stem().string() + schemeSuffix;
        string pngOutputFilename = baseNameForOutput + config.outputPngExtension;
        path finalPngOutputPath = outputSubDirPath / pngOutputFilename;

        cout << "  Processing scheme: " << colorSchemeToString(currentScheme) << " -> PNG: " << finalPngOutputPath.filename().string() << endl; // Use helper to print name

        unsigned char bgColor[3], fgColor[3];
        setSchemeColors(currentScheme, bgColor, fgColor);

        vector<unsigned char> outputImageData;
        try {
            size_t required_size = static_cast<size_t>(outputImageWidthPx) * outputImageHeightPx * OUTPUT_CHANNELS;
            if (outputImageWidthPx <= 0 || outputImageHeightPx <= 0 || static_cast<double>(outputImageWidthPx) * outputImageHeightPx > static_cast<double>(outputImageData.max_size() / OUTPUT_CHANNELS)) {
                 throw std::runtime_error("Invalid or too large PNG dimensions before allocation.");
            }
            outputImageData.resize(required_size);
        } catch (const std::exception& e) {
            cerr << "Error: Allocating PNG buffer for " << colorSchemeToString(currentScheme) << ": " << e.what() << ". Skipping." << endl;
            allSchemesSuccessful = false;
            continue; // Skip to next scheme
        }

        renderAsciiArtToImage(outputImageData, asciiResultData, fontInfo, currentScheme,
                              bgColor, fgColor, outputImageWidthPx, outputImageHeightPx,
                              ascentPx, scaleForPng, lineHeightPx);

        if (!saveImagePng(finalPngOutputPath, outputImageWidthPx, outputImageHeightPx, OUTPUT_CHANNELS, outputImageData)) {
            cerr << "  Error: Failed to save PNG for " << colorSchemeToString(currentScheme) << "." << endl;
            allSchemesSuccessful = false;
        }
    }
    auto proc_end = high_resolution_clock::now();
    cout << "-> Finished IMAGE processing '" << imagePath.filename().string() << "'. Time: "
         << fixed << setprecision(3) << duration_cast<milliseconds>(proc_end - proc_start).count() / 1000.0 << "s" << endl;
    return allSchemesSuccessful;
}


int main(int argc, char* argv[]) {
    cout << "--- ASCII Art to IMAGE (PNG) Generator ---" << endl;
    Config config; // Initialize with defaults, including default schemes
    const string configFilename = "config.ini";

    path exePath;
    try {
         if (argc > 0 && argv[0] != nullptr && !string(argv[0]).empty() && filesystem::exists(argv[0])) {
             std::error_code ec;
             path tempPath = filesystem::canonical(argv[0], ec);
             if (!ec) {
                 exePath = tempPath;
             } else {
                 path currentP = filesystem::current_path();
                 tempPath = currentP / argv[0];
                  if (filesystem::exists(tempPath)) {
                     exePath = tempPath;
                 } else {
                     #ifdef _WIN32
                         char pathBuf[MAX_PATH];
                         if (GetModuleFileNameA(NULL, pathBuf, MAX_PATH) != 0) {
                             exePath = pathBuf;
                         } else {
                             cerr << "Warning: GetModuleFileNameA failed. Error: " << GetLastError() << ". Using fallback." << endl;
                             exePath = currentP / "ascii_to_image_fallback.exe";
                         }
                     #else
                         exePath = currentP / "ascii_to_image_fallback";
                     #endif
                 }
             }
         } else {
             #ifdef _WIN32
                 char pathBuf[MAX_PATH];
                 if (GetModuleFileNameA(NULL, pathBuf, MAX_PATH) != 0) {
                      exePath = pathBuf;
                 } else {
                     cerr << "Warning: GetModuleFileNameA failed. Error: " << GetLastError() << ". Using fallback." << endl;
                     exePath = filesystem::current_path() / "ascii_to_image_fallback.exe";
                 }
             #else
                 exePath = filesystem::current_path() / "ascii_to_image";
             #endif
         }
    } catch (const filesystem::filesystem_error& fs_err) {
        cerr << "Warning: Filesystem error resolving executable path: " << fs_err.what() << ". Using current_path fallback." << endl;
        string exeName = (argc > 0 && argv[0] != nullptr && !string(argv[0]).empty()) ? filesystem::path(argv[0]).filename().string() : "ascii_to_image";
        exePath = filesystem::current_path() / exeName;
    } catch (const std::exception& e) {
        cerr << "Warning: Error resolving executable path: " << e.what() << ". Using current_path fallback." << endl;
        string exeName = (argc > 0 && argv[0] != nullptr && !string(argv[0]).empty()) ? filesystem::path(argv[0]).filename().string() : "ascii_to_image";
        exePath = filesystem::current_path() / exeName;
    }

    path exeDir = exePath.parent_path();
    path configPathObj = exeDir / configFilename;

    // Load configuration - This will potentially override config.schemesToGenerate
    loadConfiguration(configPathObj, config);

    config.finalFontPath = (exeDir / config.fontFilename).string();
     if (!filesystem::exists(config.finalFontPath)) {
        path currentDirFontPath = filesystem::current_path() / config.fontFilename;
        if (filesystem::exists(currentDirFontPath)) {
            config.finalFontPath = currentDirFontPath.string();
            cout << "Info: Font found in current working directory: " << config.finalFontPath << endl;
        } else {
            cerr << "Error: Font file '" << config.fontFilename << "' not found near executable (" << exeDir.string() << ") or in current directory (" << filesystem::current_path().string() <<")." << endl;
            cerr << "Please ensure '" << config.fontFilename << "' is correctly placed or provide a full path in " << configFilename << "." << endl;
            return 1;
        }
    }


    cout << "\n--- PNG Output Configuration ---" << endl;
    cout << "Target Width (Chars): " << config.targetWidth << endl;
    cout << "Aspect Correction: " << config.charAspectRatioCorrection << endl;
    cout << "Font: " << config.finalFontPath << " (Size for PNG: " << config.fontSize << "px)" << endl;
    cout << "Tiled Rendering: " << (config.enableTiledRendering ? "true" : "false") << " (Tile Size: " << config.tileSize << ")" << endl;

    // Print the final list of schemes to be generated
    cout << "Color Schemes to Generate: ";
    if (config.schemesToGenerate.empty()) {
        cout << " (None - Check config or defaults)";
    } else {
        for (size_t i = 0; i < config.schemesToGenerate.size(); ++i) {
            cout << colorSchemeToString(config.schemesToGenerate[i]) << (i == config.schemesToGenerate.size() - 1 ? "" : ", ");
        }
    }
    cout << endl;
    cout << "-----------------------------" << endl;


    stbtt_fontinfo fontInfo;
    vector<unsigned char> fontBuffer;
    if (!loadFontInfo(config.finalFontPath, fontInfo, fontBuffer)) {
        return 1;
    }

    string inputPathStr = getUserInputPath();
    if (inputPathStr.empty()) return 1;
    path inputPath(inputPathStr);

    int processedCount = 0;
    int failedCount = 0;
    auto overall_start_time = high_resolution_clock::now();

    try {
        if (!filesystem::exists(inputPath)) {
            cerr << "Error: Input path does not exist: " << inputPath.string() << endl;
            return 1;
        }

        if (filesystem::is_regular_file(inputPath)) {
            cout << "\nInput is a single file." << endl;
            if (isImageFile(inputPath)) {
                if (processImageFileForPng(inputPath, inputPath.parent_path(), config, fontInfo, fontBuffer)) {
                    processedCount++;
                } else {
                    failedCount++;
                }
            } else {
                cerr << "Error: Not a supported image file: " << inputPath.string() << endl;
                failedCount++;
            }
        } else if (filesystem::is_directory(inputPath)) {
            cout << "\nInput is a directory. Processing images concurrently..." << endl;
            string batchDirName = inputPath.filename().string() + "_" + std::to_string(config.targetWidth) + config.batchOutputSubDirSuffix + "_PNG";
            path batchBaseOutputDir = inputPath.parent_path() / batchDirName;
             try {
                if(filesystem::create_directories(batchBaseOutputDir)) {
                    cout << "Created main batch output directory: " << batchBaseOutputDir.string() << endl;
                } else if (!filesystem::is_directory(batchBaseOutputDir)) {
                     cerr << "Error: Failed to create/access batch output directory: " << batchBaseOutputDir.string() << endl;
                     return 1;
                }
            } catch (const filesystem::filesystem_error& e) {
                cerr << "Error creating batch output directory " << batchBaseOutputDir.string() << ": " << e.what() << endl;
                return 1;
            }

            vector<path> imageFilesToProcess;
            for (const auto& entry : filesystem::directory_iterator(inputPath)) {
                if (entry.is_regular_file() && isImageFile(entry.path())) {
                    imageFilesToProcess.push_back(entry.path());
                }
            }

            if (imageFilesToProcess.empty()) {
                cout << "No supported image files found in: " << inputPath.string() << endl;
            } else {
                cout << "Found " << imageFilesToProcess.size() << " image(s) to process for PNG output." << endl;
                vector<std::future<bool>> futures;
                futures.reserve(imageFilesToProcess.size());
                for(const auto& imgPath : imageFilesToProcess) {
                    futures.push_back(
                        std::async(std::launch::async,
                                   processImageFileForPng,
                                   imgPath, batchBaseOutputDir,
                                   std::cref(config), std::cref(fontInfo), std::cref(fontBuffer))
                    );
                }
                for (size_t i = 0; i < futures.size(); ++i) {
                    try {
                        if (futures[i].get()) processedCount++; else failedCount++;
                    } catch (const std::exception& e) {
                        cerr << "Error processing (PNG) '" << imageFilesToProcess[i].filename().string() << "': " << e.what() << endl;
                        failedCount++;
                    }
                }
            }
        } else {
            cerr << "Error: Input path is not a file or directory: " << inputPath.string() << endl;
            return 1;
        }
    } catch (const std::exception& e) {
        cerr << "An unexpected error occurred: " << e.what() << endl;
        return 1;
    }

    auto overall_end_time = high_resolution_clock::now();
    cout << "\n==================================================" << endl;
    cout << "PNG Generation Summary:" << endl;
    cout << "  Successfully processed: " << processedCount << " image(s)" << endl;
    cout << "  Failed/Skipped:       " << failedCount << " image(s)" << endl;
    cout << "  Total time:           " << fixed << setprecision(3)
         << duration_cast<duration<double>>(overall_end_time - overall_start_time).count() << "s" << endl;
    cout << "==================================================" << endl;

    cout << "\nPress Enter to exit..." << endl;
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cin.get();
    return (failedCount > 0);
}
