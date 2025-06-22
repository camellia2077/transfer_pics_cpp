// config_handler.cpp
// 专门负责读取和解析 config.json
#include "config_handler.h"
#include "common_types.h"
#include <nlohmann/json.hpp> // 使用 nlohmann/json 库
#include <iostream>
#include <fstream>
#include <iomanip>

// 使用 nlohmann::json 的命名空间
using json = nlohmann::json;

// 定义颜色方案名称到枚举的映射
const std::unordered_map<std::string, ColorScheme> g_schemeNameMap = {
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

const std::unordered_map<std::string, ColorScheme>& getColorSchemeMap() {
    return g_schemeNameMap;
}


// --- Public Functions ---

bool loadConfiguration(const std::filesystem::path& configPath, Config& config) {
    std::cout << "Info: Attempting to load configuration from '" << configPath.string() << "'..." << std::endl;
    
    std::ifstream configFile(configPath);
    if (!configFile.is_open()) {
        std::cout << "Info: Config file '" << configPath.string() << "' not found. Using default values." << std::endl;
        return true; // 文件不存在是正常情况，使用默认配置
    }

    try {
        json j;
        configFile >> j; // 从文件流解析 JSON

        // 安全地获取 "Settings" 对象
        const auto& settings = j.value("Settings", json::object());

        // 使用 .value() 方法安全地读取每个配置项，如果键不存在则使用默认值
        config.targetWidth = settings.value("targetWidth", config.targetWidth);
        config.charAspectRatioCorrection = settings.value("charAspectRatioCorrection", config.charAspectRatioCorrection);
        config.fontFilename = settings.value("fontFilename", config.fontFilename);
        config.fontSize = settings.value("fontSize", config.fontSize);
        config.enableTiledRendering = settings.value("enableTiledRendering", config.enableTiledRendering);
        config.tileSize = settings.value("tileSize", config.tileSize);
        config.outputPngExtension = settings.value("outputPngExtension", config.outputPngExtension);
        config.imageOutputSubDirSuffix = settings.value("imageOutputSubDirSuffix", config.imageOutputSubDirSuffix);
        config.batchOutputSubDirSuffix = settings.value("batchOutputSubDirSuffix", config.batchOutputSubDirSuffix);
        
        // HTML 相关配置
        config.generateHtmlOutput = settings.value("generateHtmlOutput", config.generateHtmlOutput);
        config.htmlFontSizePt = settings.value("htmlFontSizePt", config.htmlFontSizePt);
        config.outputHtmlExtension = settings.value("outputHtmlExtension", config.outputHtmlExtension);
        
        // 处理颜色方案数组
        if (settings.contains("colorSchemes") && settings["colorSchemes"].is_array()) {
            config.schemesToGenerate.clear();
            const auto& map = getColorSchemeMap();
            for (const auto& schemeNameJson : settings["colorSchemes"]) {
                if (schemeNameJson.is_string()) {
                    string schemeNameStr = schemeNameJson.get<string>();
                    string lowerSchemeName = toLower(schemeNameStr);
                    auto it = map.find(lowerSchemeName);
                    if (it != map.end()) {
                        config.schemesToGenerate.push_back(it->second);
                    } else {
                        std::cerr << "Warning: Unknown color scheme name in config: '" << schemeNameStr << "'. Ignoring." << std::endl;
                    }
                }
            }
        }
        
        // 如果加载后列表为空，则恢复默认值
        if (config.schemesToGenerate.empty()) {
            std::cerr << "Warning: No valid color schemes found in config. Reverting to defaults." << std::endl;
            config.schemesToGenerate = { ColorScheme::BLACK_ON_WHITE, ColorScheme::COLOR_ON_WHITE };
        }

    } catch (const json::parse_error& e) {
        // 捕获 JSON 解析错误
        std::cerr << "Error: Failed to parse config file '" << configPath.string() << "'." << std::endl;
        std::cerr << "       Reason: " << e.what() << std::endl;
        std::cerr << "       Using default values instead." << std::endl;
        return false;
    } catch (const std::exception& e) {
        // 捕获其他可能的异常
        std::cerr << "An unexpected error occurred while reading config: " << e.what() << std::endl;
        return false;
    }

    std::cout << "Info: Configuration loaded successfully." << std::endl;
    return true;
}


// 此函数保持不变，因为它用于生成一个人类可读的运行日志，而不是一个有效的JSON文件。
bool writeConfigToFile(const Config& config, const std::filesystem::path& outputFilePath) {
    std::ofstream configFile(outputFilePath);
    if (!configFile.is_open()) {
        std::cerr << "Error: Could not open config output file for writing: " << outputFilePath.string() << std::endl;
        return false;
    }

    std::cout << "Info: Writing effective configuration to: " << outputFilePath.string() << std::endl;

    configFile << "# Effective configuration used for this run" << std::endl;
    configFile << "# Automatically generated by the program." << std::endl;
    configFile << std::endl;

    configFile << "[Settings]" << std::endl;
    configFile << "targetWidth = " << config.targetWidth << std::endl;
    configFile << "charAspectRatioCorrection = " << std::fixed << std::setprecision(6) << config.charAspectRatioCorrection << std::endl;
    configFile << "fontFilename = " << config.fontFilename << "  # Relative path specified in config.json" << std::endl;
    configFile << "finalFontPath = " << config.finalFontPath << "  # Resolved absolute/relative path used" << std::endl;
    configFile << "fontSize = " << std::fixed << std::setprecision(2) << config.fontSize << " # Font size for PNG output" << std::endl;
    configFile << "enableTiledRendering = " << (config.enableTiledRendering ? "true" : "false") << std::endl;
    configFile << "tileSize = " << config.tileSize << std::endl;
    configFile << "outputPngExtension = " << config.outputPngExtension << std::endl;
    configFile << "imageOutputSubDirSuffix = " << config.imageOutputSubDirSuffix << std::endl;
    configFile << "batchOutputSubDirSuffix = " << config.batchOutputSubDirSuffix << std::endl;

    // HTML Settings
    configFile << "generateHtmlOutput = " << (config.generateHtmlOutput ? "true" : "false") << std::endl;
    configFile << "htmlFontSizePt = " << std::fixed << std::setprecision(2) << config.htmlFontSizePt << " # Font size for HTML output in points" << std::endl;
    configFile << "outputHtmlExtension = " << config.outputHtmlExtension << std::endl;


    configFile << "colorSchemes = ";
    if (config.schemesToGenerate.empty()) {
        configFile << "# (None specified or loaded)" << std::endl;
    } else {
        for (size_t i = 0; i < config.schemesToGenerate.size(); ++i) {
            configFile << colorSchemeToString(config.schemesToGenerate[i]); 
            if (i < config.schemesToGenerate.size() - 1) {
                configFile << ", ";
            }
        }
        configFile << " # List of schemes generated in this run" << std::endl;
    }

    configFile.close();
    if (!configFile) { 
         std::cerr << "Error: Failed to write all data or close the config output file: " << outputFilePath.string() << std::endl;
         return false;
    }

    return true;
}


// 定义在 common_types.h 中声明的辅助函数
string colorSchemeToString(ColorScheme scheme) {
    // ... 此函数内容与之前版本相同 ...
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

string getSchemeSuffix(ColorScheme scheme) {
    // ... 此函数内容与之前版本相同 ...
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
