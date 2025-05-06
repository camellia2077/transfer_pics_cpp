// ascii_to_pdf.cpp
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
#include <sstream>      // For stringstream
#include <cctype>       // For tolower
#include <set>          // For supported extensions
#include <stdexcept>    // For runtime_error in PDF
#include <future>       // For std::async, std::future
#include <thread>       // For std::thread::hardware_concurrency
#include <functional>   // For std::cref

// --- Add libHaru Header ---
#ifdef _WIN32
#include <windows.h> // May be needed by hpdf.h
#endif
#include <hpdf.h> // Needs libHaru installed and in include path

// Define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // Needs to be in your include path or project

// Define STB_TRUETYPE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h" // Needs to be in your include path or project

#define INI_IMPLEMENTATION
#include "ini.h" // Needs to be in your include path or project

using namespace std;
using std::filesystem::path;
using namespace std::chrono;

// --- Common Constants, Types, and Structures (Ideally in a shared header) ---
const string ASCII_CHARS = "@%#*+=-:. ";
const int NUM_ASCII_CHARS = static_cast<int>(ASCII_CHARS.length());
const int IMAGE_INPUT_CHANNELS = 3; // For loading images

const set<string> SUPPORTED_EXTENSIONS = {
    ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif"
};
const float POINTS_PER_INCH = 72.0f;


enum class ColorScheme {
    AMBER_ON_BLACK, BLACK_ON_YELLOW, BLACK_ON_CYAN, COLOR_ON_WHITE,
    COLOR_ON_BLACK, CYAN_ON_BLACK, GRAY_ON_BLACK, GREEN_ON_BLACK,
    MAGENTA_ON_BLACK, PURPLE_ON_BLACK, SEPIA, SOLARIZED_DARK,
    SOLARIZED_LIGHT, WHITE_ON_BLACK, WHITE_ON_BLUE, WHITE_ON_DARK_RED,
    YELLOW_ON_BLACK, BLACK_ON_WHITE,
};

struct CharColorInfo {
    char character;
    unsigned char color[3];
};

struct Config {
    int targetWidth = 1024;
    double charAspectRatioCorrection = 2.0;
    string fontFilename = "Consolas.ttf";
    // float fontSize = 15.0f; // Not used by this program directly for rendering
    float pdfFontSize = 10.0f; // PDF font size
    string finalFontPath = "";
    // bool enableTiledRendering = false; // Not relevant for PDF text output
    // int tileSize = 512; // Not relevant
    string baseOutputFilename = "output_ascii_art_pdf"; // Not directly used
    // string outputPngExtension = ".png"; // Not used
    string outputPdfExtension = ".pdf";
    string imageOutputSubDirSuffix = "_ascii_output_pdf"; // Suffix for PDF outputs
    string batchOutputSubDirSuffix = "_ascii_batch_output_pdf";
    vector<ColorScheme> schemesToGenerate = {
        ColorScheme::BLACK_ON_WHITE,
        ColorScheme::COLOR_ON_WHITE,
    };
};

// --- Common INI Handler (Ideally in a shared .cpp, declared in shared header) ---
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
        } else if (strcmp(name, "pdfFontSize") == 0) { // PDF font size
            pconfig->pdfFontSize = stof(value);
            LOG_LOADED(pdfFontSize (PDF), pconfig->pdfFontSize);
        }
        // fontSize (for PNG) would be ignored or handled gracefully
    } catch (const std::invalid_argument& e) {
        cerr << "Warning: Invalid data for key '" << name << "': \"" << value << "\". Error: " << e.what() << endl;
        return 0;
    } catch (const std::out_of_range& e) {
        cerr << "Warning: Value out of range for key '" << name << "': \"" << value << "\". Error: " << e.what() << endl;
        return 0;
    }
    return 1;
}

// --- libHaru Error Handler ---
#ifdef HPDF_DLL
void __stdcall
#else
void
#endif
error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void *user_data) {
    cerr << "PDF ERROR: error_no=" << error_no << ", detail_no=" << detail_no << endl;
    // Consider throwing an exception here to stop PDF generation for the current file
    // For now, just prints. Robust code might throw std::runtime_error.
}


// --- Common Utility Functions (Ideally in shared .h/.cpp) ---
// trim, isImageFile, read_file, getSchemeSuffix, loadConfiguration, getUserInputPath
// setupImageOutputSubdirectory, loadImage, generateAsciiData, loadFontInfo, setSchemeColors
// are identical to the image program. For brevity, they are not repeated here but should be included.
// ... (Paste the common utility functions from ascii_to_image.cpp here) ...
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
    cout << "Info: Attempting to load configuration from '" << configPath.string() << "'..." << endl;
    int parseResult = ini_parse(configPath.string().c_str(), iniHandler, &config);
    if (parseResult < 0) {
        cout << "Info: Config file '" << configPath.string() << "' not found. Using defaults." << endl;
        return true;
    } else if (parseResult > 0) {
        cerr << "Warning: Error parsing config '" << configPath.string() << "' at line " << parseResult << ". Using defaults for failed settings." << endl;
        return false;
    }
    cout << "Info: Configuration loaded successfully." << endl;
    return true;
}

string getUserInputPath() {
    string inputPathStr;
    cout << "Please enter the image file or folder path for PDF generation: ";
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
    unsigned char *data = stbi_load(imagePath.c_str(), &width, &height, nullptr, IMAGE_INPUT_CHANNELS);
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

            size_t pixelOffset = (static_cast<size_t>(yImg) * width + xImg) * IMAGE_INPUT_CHANNELS;
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
    fontBuffer = read_file(fontPath); // fontBuffer IS needed for embedding in PDF by libHaru
    if (fontBuffer.empty()) return false;
    // stbtt_InitFont is primarily for STB rasterization, not strictly needed if only using libHaru's font loading,
    // but good for consistency or if you wanted to get metrics via STB first.
    if (!stbtt_InitFont(&fontInfo, fontBuffer.data(), stbtt_GetFontOffsetForIndex(fontBuffer.data(), 0))) {
        cerr << "Warning: Failed to initialize font with STB (may be okay for PDF if libHaru loads it)." << endl;
        // Not returning false, as libHaru might still load it.
    }
    cout << "Font file read successfully for PDF embedding. Location: " << fontPath << endl;
    return true;
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


// --- PDF Program Specific Functions ---
bool saveAsciiArtToPdf(const path& outputPath,
                       const vector<vector<CharColorInfo>>& asciiData,
                       const Config& config, // For font path, pdf font size
                       ColorScheme scheme,
                       const unsigned char bgColorUint[3],
                       const unsigned char fgColorUint[3]) {
    HPDF_Doc pdf;
    HPDF_Page page;
    HPDF_Font pdfFont;
    const char* fontNameCStr;

    pdf = HPDF_New(error_handler, nullptr);
    if (!pdf) {
        cerr << "PDF Error: Cannot create HPDF_Doc object." << endl;
        return false;
    }

    try {
        fontNameCStr = HPDF_LoadTTFontFromFile(pdf, config.finalFontPath.c_str(), HPDF_TRUE); // HPDF_TRUE to embed
        if (!fontNameCStr) {
            cerr << "PDF Error: Cannot load font: " << config.finalFontPath << ". Trying Helvetica." << endl;
            fontNameCStr = "Helvetica"; // Fallback
        }
        pdfFont = HPDF_GetFont(pdf, fontNameCStr, nullptr); // Use default encoding
        if (!pdfFont) {
            cerr << "PDF Error: Cannot get font: " << fontNameCStr << endl;
            throw runtime_error("Failed to get PDF font.");
        }

        page = HPDF_AddPage(pdf);
        if (!page) throw runtime_error("Failed to add PDF page.");

        // Estimate page dimensions (simple approach)
        float pdfCharWidthPt = HPDF_Font_TextWidth(pdfFont, (const HPDF_BYTE*)"M", 1).width * config.pdfFontSize / 1000.0f;
        if (pdfCharWidthPt <= 0) pdfCharWidthPt = config.pdfFontSize * 0.6f; // Fallback
        float pdfLineHeightPt = config.pdfFontSize * 1.2f; // A common multiplier for line height

        float margin = 20.0f; // Points
        HPDF_REAL page_width = static_cast<HPDF_REAL>(asciiData[0].size() * pdfCharWidthPt + 2 * margin);
        HPDF_REAL page_height = static_cast<HPDF_REAL>(asciiData.size() * pdfLineHeightPt + 2 * margin);

        HPDF_Page_SetWidth(page, page_width);
        HPDF_Page_SetHeight(page, page_height);

        // Background
        HPDF_Page_SetRGBFill(page, bgColorUint[0] / 255.0f, bgColorUint[1] / 255.0f, bgColorUint[2] / 255.0f);
        HPDF_Page_Rectangle(page, 0, 0, page_width, page_height);
        HPDF_Page_Fill(page);

        HPDF_Page_BeginText(page);
        HPDF_Page_SetFontAndSize(page, pdfFont, config.pdfFontSize);
        HPDF_Page_SetTextLeading(page, pdfLineHeightPt); // Set line spacing

        HPDF_REAL startX = margin;
        HPDF_REAL startY = page_height - margin - config.pdfFontSize; // Top-leftish start
        HPDF_Page_MoveTextPos(page, startX, startY);

        for (const auto& lineData : asciiData) {
            // For precise horizontal positioning if ShowText isn't enough for monospaced look:
            // float current_x_pos = startX;
            // HPDF_Page_MoveTextPos(page, current_x_pos, HPDF_Page_GetCurrentTextPos(page).y);

            for (const auto& charInfo : lineData) {
                const unsigned char* finalColor = (scheme == ColorScheme::COLOR_ON_WHITE || scheme == ColorScheme::COLOR_ON_BLACK) ? charInfo.color : fgColorUint;
                HPDF_Page_SetRGBFill(page, finalColor[0] / 255.0f, finalColor[1] / 255.0f, finalColor[2] / 255.0f);
                
                char charStr[2] = { charInfo.character, '\0' };
                HPDF_Page_ShowText(page, charStr);
                // If not using ShowText's auto-advance for fixed width:
                // current_x_pos += pdfCharWidthPt;
                // HPDF_Page_MoveTextPos(page, current_x_pos, HPDF_Page_GetCurrentTextPos(page).y);
            }
            HPDF_Page_MoveToNextLine(page); // Moves to startX of next line based on leading
        }

        HPDF_Page_EndText(page);
        if (HPDF_SaveToFile(pdf, outputPath.string().c_str()) != HPDF_OK) {
            throw runtime_error("Failed to save PDF file.");
        }

    } catch (const std::exception& e) {
        cerr << "PDF Exception: " << e.what() << endl;
        HPDF_Free(pdf);
        return false;
    } catch (...) {
        cerr << "PDF Error: Unknown exception during PDF generation." << endl;
        HPDF_Free(pdf);
        return false;
    }

    HPDF_Free(pdf);
    return true;
}


bool processImageFileForPdf(const path& imagePath,
                            const path& baseOutputDir,
                            const Config& config,
                            const stbtt_fontinfo& fontInfo, // For consistency, though libHaru re-loads
                            const vector<unsigned char>& fontBuffer) // Font data buffer for embedding
{
    cout << "\n==================================================" << endl;
    cout << "Processing for PDF: " << imagePath.string() << endl;
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
    auto imgDataPtr = loadImage(imagePath.string(), width, height); // Uses IMAGE_INPUT_CHANNELS
    auto load_end = high_resolution_clock::now();
     cout << "-> Loaded (" << width << "x" << height << "), Time: "
         << fixed << setprecision(3) << duration_cast<milliseconds>(load_end - load_start).count() / 1000.0 << "s" << endl;
    if (!imgDataPtr) return false;

    auto proc_start = high_resolution_clock::now();
    cout << "Generating ASCII data..." << endl;
    int targetHeight = static_cast<int>(std::round(static_cast<double>(height * config.targetWidth) / (width * config.charAspectRatioCorrection)));
    targetHeight = max(1, targetHeight);
    cout << "Calculated ASCII grid: " << config.targetWidth << "x" << targetHeight << endl;

    vector<vector<CharColorInfo>> asciiResultData = generateAsciiData(
        imgDataPtr.get(), width, height, config.targetWidth, targetHeight);
    if (asciiResultData.empty() || asciiResultData[0].empty()) {
        cerr << "Error: Failed to generate ASCII data for " << imagePath.filename().string() << ". Skipping." << endl;
        return false;
    }

    bool allSchemesSuccessful = true;
    for (const auto& currentScheme : config.schemesToGenerate) {
        string schemeSuffix = getSchemeSuffix(currentScheme);
        string baseNameForOutput = imagePath.stem().string() + schemeSuffix;
        string pdfOutputFilename = baseNameForOutput + config.outputPdfExtension;
        path finalPdfOutputPath = outputSubDirPath / pdfOutputFilename;

        cout << "  Processing scheme: " << schemeSuffix.substr(1) << " -> PDF: " << finalPdfOutputPath.filename().string() << endl;

        unsigned char bgColor[3], fgColor[3];
        setSchemeColors(currentScheme, bgColor, fgColor);

        if (!saveAsciiArtToPdf(finalPdfOutputPath, asciiResultData, config, currentScheme, bgColor, fgColor)) {
            cerr << "  Error: Failed to save PDF for " << schemeSuffix.substr(1) << "." << endl;
            allSchemesSuccessful = false;
        }
    }
    auto proc_end = high_resolution_clock::now();
    cout << "-> Finished PDF processing '" << imagePath.filename().string() << "'. Time: "
         << fixed << setprecision(3) << duration_cast<milliseconds>(proc_end - proc_start).count() / 1000.0 << "s" << endl;
    return allSchemesSuccessful;
}


int main(int argc, char* argv[]) {
    cout << "--- ASCII Art to PDF Generator ---" << endl;
    Config config;
    const string configFilename = "config.ini"; // Assumed to be same config file

    path exePath;
     try {
        if (argc > 0 && argv[0] != nullptr && filesystem::exists(argv[0])) {
            exePath = filesystem::canonical(argv[0]);
        } else {
            #ifdef _WIN32
                char pathBuf[MAX_PATH];
                GetModuleFileNameA(NULL, pathBuf, MAX_PATH); // Windows specific
                exePath = pathBuf;
            #else
                exePath = filesystem::current_path() / "ascii_to_pdf"; // Fallback
            #endif
        }
    } catch (const std::exception& e) {
        cerr << "Warning: Error resolving executable path: " << e.what() << ". Using current dir." << endl;
        exePath = filesystem::current_path() / (argc > 0 && argv[0] ? filesystem::path(argv[0]).filename() : "ascii_to_pdf");
    }
    path exeDir = exePath.parent_path();
    path configPathObj = exeDir / configFilename;

    loadConfiguration(configPathObj, config); // Loads common settings + pdfFontSize
    config.finalFontPath = (exeDir / config.fontFilename).string();
    if (!filesystem::exists(config.finalFontPath)) {
        cerr << "Error: Font file '" << config.finalFontPath << "' not found (needed for PDF embedding)." << endl;
        return 1;
    }

    cout << "\n--- PDF Output Configuration ---" << endl;
    cout << "Target Width (Chars): " << config.targetWidth << endl;
    cout << "Aspect Correction: " << config.charAspectRatioCorrection << endl;
    cout << "Font: " << config.finalFontPath << " (Size for PDF: " << config.pdfFontSize << "pt)" << endl;
    cout << "----------------------------" << endl;

    stbtt_fontinfo fontInfo; // May not be heavily used if libHaru handles all font metrics
    vector<unsigned char> fontBuffer; // IMPORTANT: fontBuffer must contain the font file data for embedding
    if (!loadFontInfo(config.finalFontPath, fontInfo, fontBuffer)) { // Ensures fontBuffer is loaded
        cerr << "Failed to read font file into buffer for PDF embedding." << endl;
        return 1;
    }
    if (fontBuffer.empty()) { // Double check
        cerr << "Error: Font buffer is empty after loadFontInfo. Cannot embed in PDF." << endl;
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
                if (processImageFileForPdf(inputPath, inputPath.parent_path(), config, fontInfo, fontBuffer)) {
                    processedCount++;
                } else {
                    failedCount++;
                }
            } else {
                cerr << "Error: Not a supported image file: " << inputPath.string() << endl;
                failedCount++;
            }
        } else if (filesystem::is_directory(inputPath)) {
            cout << "\nInput is a directory. Processing images for PDF concurrently..." << endl;
            string batchDirName = inputPath.filename().string() + "_" + std::to_string(config.targetWidth) + config.batchOutputSubDirSuffix + "_PDF";
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
                cout << "Found " << imageFilesToProcess.size() << " image(s) to process for PDF output." << endl;
                vector<std::future<bool>> futures;
                futures.reserve(imageFilesToProcess.size());
                for(const auto& imgPath : imageFilesToProcess) {
                    futures.push_back(
                        std::async(std::launch::async,
                                   processImageFileForPdf,
                                   imgPath, batchBaseOutputDir,
                                   std::cref(config), std::cref(fontInfo), std::cref(fontBuffer)) // Pass fontBuffer by const ref
                    );
                }
                for (size_t i = 0; i < futures.size(); ++i) {
                    try {
                        if (futures[i].get()) processedCount++; else failedCount++;
                    } catch (const std::exception& e) {
                        cerr << "Error processing for PDF '" << imageFilesToProcess[i].filename().string() << "': " << e.what() << endl;
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
    cout << "PDF Generation Summary:" << endl;
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