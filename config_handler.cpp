// config_handler.cpp
// 专门负责读取和解析 config.ini
#include "config_handler.h"
#include "common_types.h" // Include again for direct access if needed, though config_handler.h includes it
#include <ini.h>         // inih library header
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>       // For strcmp

// Define the map here
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


// --- INI Handler ---
static int iniHandler(void* user, const char* section, const char* name, const char* value_cstr) {
    Config* pconfig = static_cast<Config*>(user);
    string value(value_cstr);

    if (strcmp(section, "Settings") != 0) {
        return 1; // Skip sections other than [Settings]
    }

    #define LOG_LOADED(key, val) std::cout << "  -> Loaded " #key " = " << val << std::endl

    try {
        if (strcmp(name, "targetWidth") == 0) {
            pconfig->targetWidth = std::stoi(value);
            LOG_LOADED(targetWidth, pconfig->targetWidth);
        } else if (strcmp(name, "charAspectRatioCorrection") == 0) {
            pconfig->charAspectRatioCorrection = std::stod(value);
            LOG_LOADED(charAspectRatioCorrection, pconfig->charAspectRatioCorrection);
        } else if (strcmp(name, "fontFilename") == 0) {
            if (!value.empty()) {
                pconfig->fontFilename = value;
                LOG_LOADED(fontFilename, pconfig->fontFilename);
            } else {
                std::cerr << "Warning: Empty value for 'fontFilename'. Using default." << std::endl;
            }
        } else if (strcmp(name, "fontSize") == 0) {
            pconfig->fontSize = std::stof(value);
            LOG_LOADED(fontSize (PNG), pconfig->fontSize);
        } else if (strcmp(name, "enableTiledRendering") == 0) {
            string lower_val = toLower(value);
            if (lower_val == "true" || lower_val == "yes" || lower_val == "1") pconfig->enableTiledRendering = true;
            else if (lower_val == "false" || lower_val == "no" || lower_val == "0") pconfig->enableTiledRendering = false;
            else {
                std::cerr << "Warning: Invalid boolean for 'enableTiledRendering': \"" << value << "\". Using default." << std::endl;
                return 0;
            }
            LOG_LOADED(enableTiledRendering, pconfig->enableTiledRendering);
        } else if (strcmp(name, "tileSize") == 0) {
            pconfig->tileSize = std::stoi(value);
            if (pconfig->tileSize <= 0) {
                std::cerr << "Warning: tileSize must be positive. Using default 512." << std::endl;
                pconfig->tileSize = 512;
            }
            LOG_LOADED(tileSize, pconfig->tileSize);
        } else if (strcmp(name, "outputPngExtension") == 0) {
             pconfig->outputPngExtension = value;
             LOG_LOADED(outputPngExtension, pconfig->outputPngExtension);
        } else if (strcmp(name, "imageOutputSubDirSuffix") == 0) {
             pconfig->imageOutputSubDirSuffix = value;
             LOG_LOADED(imageOutputSubDirSuffix, pconfig->imageOutputSubDirSuffix);
        } else if (strcmp(name, "batchOutputSubDirSuffix") == 0) {
             pconfig->batchOutputSubDirSuffix = value;
             LOG_LOADED(batchOutputSubDirSuffix, pconfig->batchOutputSubDirSuffix);
        }
        // HTML Specific Settings
        else if (strcmp(name, "generateHtmlOutput") == 0) {
            string lower_val = toLower(value);
            if (lower_val == "true" || lower_val == "yes" || lower_val == "1") pconfig->generateHtmlOutput = true;
            else if (lower_val == "false" || lower_val == "no" || lower_val == "0") pconfig->generateHtmlOutput = false;
            else {
                std::cerr << "Warning: Invalid boolean for 'generateHtmlOutput': \"" << value << "\". Using default." << std::endl;
            }
            LOG_LOADED(generateHtmlOutput, pconfig->generateHtmlOutput);
        } else if (strcmp(name, "htmlFontSizePt") == 0) {
            pconfig->htmlFontSizePt = std::stof(value);
            LOG_LOADED(htmlFontSizePt, pconfig->htmlFontSizePt);
        } else if (strcmp(name, "outputHtmlExtension") == 0) {
             pconfig->outputHtmlExtension = value;
             LOG_LOADED(outputHtmlExtension, pconfig->outputHtmlExtension);
        }
        // --- Handle colorSchemes ---
        else if (strcmp(name, "colorSchemes") == 0) {
             if (!value.empty()) {
                 pconfig->schemesToGenerate.clear(); 
                 std::stringstream ss(value);
                 std::string schemeNameStr;
                 int loadedCount = 0;
                 string loadedSchemeNamesStr;

                 while (std::getline(ss, schemeNameStr, ',')) {
                     size_t first = schemeNameStr.find_first_not_of(" \t\n\r\f\v");
                     if (string::npos == first) continue;
                     size_t last = schemeNameStr.find_last_not_of(" \t\n\r\f\v");
                     schemeNameStr = schemeNameStr.substr(first, (last - first + 1));

                     if (!schemeNameStr.empty()) {
                         std::string lowerSchemeName = toLower(schemeNameStr);
                         const auto& map = getColorSchemeMap(); 
                         auto it = map.find(lowerSchemeName);
                         if (it != map.end()) {
                             pconfig->schemesToGenerate.push_back(it->second);
                             if (loadedCount > 0) loadedSchemeNamesStr += ", ";
                             loadedSchemeNamesStr += schemeNameStr; 
                             loadedCount++;
                         } else {
                             std::cerr << "Warning: Unknown color scheme name in config: '" << schemeNameStr << "'. Ignoring." << std::endl;
                         }
                     }
                 }

                 if (loadedCount > 0) {
                      std::cout << "  -> Loaded colorSchemes = " << loadedSchemeNamesStr << std::endl;
                 } else {
                      std::cerr << "Warning: No valid color schemes found for 'colorSchemes' key. Reverting to defaults." << std::endl;
                      pconfig->schemesToGenerate = { ColorScheme::BLACK_ON_WHITE, ColorScheme::COLOR_ON_WHITE };
                      std::cout << "  -> Reverted to default schemes." << std::endl;
                 }
             } else {
                 std::cerr << "Warning: Empty value for 'colorSchemes' in config. Using default schemes." << std::endl;
             }
         }
        else {
             return 1; 
        }

    } catch (const std::invalid_argument& e) {
        std::cerr << "Warning: Invalid data format for key '" << name << "' in config.ini. Value: \"" << value << "\". Error: " << e.what() << std::endl;
        return 0; 
    } catch (const std::out_of_range& e) {
        std::cerr << "Warning: Value out of range for key '" << name << "' in config.ini. Value: \"" << value << "\". Error: " << e.what() << std::endl;
        return 0; 
    }
    return 1; 
}

// --- Public Functions ---

bool loadConfiguration(const std::filesystem::path& configPath, Config& config) {
    std::cout << "Info: Attempting to load configuration from '" << configPath.string() << "'..." << std::endl;
    int parseResult = ini_parse(configPath.string().c_str(), iniHandler, &config);

    if (parseResult < 0) {
        std::cout << "Info: Config file '" << configPath.string() << "' not found or cannot be opened. Using defaults." << std::endl;
        return true;
    } else if (parseResult > 0) {
        std::cerr << "Warning: Error parsing config '" << configPath.string() << "' at line " << parseResult << "." << std::endl;
        std::cerr << "Warning: Check format/values. Settings loaded before error and defaults for others will be used." << std::endl;
         if (config.schemesToGenerate.empty()) {
             std::cerr << "Warning: Reverting to default color schemes due to potential parsing issue." << std::endl;
             config.schemesToGenerate = { ColorScheme::BLACK_ON_WHITE, ColorScheme::COLOR_ON_WHITE };
         }
        return false; 
    }

     if (config.schemesToGenerate.empty()) {
         std::cerr << "Warning: No color schemes were successfully loaded from config. Reverting to defaults." << std::endl;
         config.schemesToGenerate = { ColorScheme::BLACK_ON_WHITE, ColorScheme::COLOR_ON_WHITE };
     }

    std::cout << "Info: Configuration loaded/parsed successfully (or defaults used)." << std::endl;
    return true; 
}


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
    configFile << "fontFilename = " << config.fontFilename << "  # Relative path specified in config.ini" << std::endl;
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


// Define the helper functions declared in common_types.h
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
