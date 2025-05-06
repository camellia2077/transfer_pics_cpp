// config_handler.h
#ifndef CONFIG_HANDLER_H
#define CONFIG_HANDLER_H

#include "common_types.h" // Includes filesystem, string, vector, etc.
#include <filesystem>

// Loads configuration from the specified INI file path into the Config struct.
// Returns true on success or if the file doesn't exist (using defaults).
// Returns false if parsing errors occur (warnings are printed).
bool loadConfiguration(const std::filesystem::path& configPath, Config& config);

// Writes the current configuration to a text file.
// Returns true on success, false on failure.
bool writeConfigToFile(const Config& config, const std::filesystem::path& outputFilePath);

// Definition needed for common_types.h helpers
string colorSchemeToString(ColorScheme scheme);
string getSchemeSuffix(ColorScheme scheme);
const std::unordered_map<std::string, ColorScheme>& getColorSchemeMap();


#endif // CONFIG_HANDLER_H