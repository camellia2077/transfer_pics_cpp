// src/ui/cli_handler.cpp

#include "cli_handler.h"
#include <iostream>
#include <iomanip>
#include <limits>

namespace CLIHandler {

void printWelcomeMessage() {
    std::cout << "--- ASCII Art Generator ---" << std::endl;
}

// 新增：实现 printUsage 函数
void printUsage(const std::string& programName) {
    std::cerr << "\nA command-line tool to convert images to ASCII art (PNG and HTML)." << std::endl;
    std::cerr << "\nUsage:\n  " << programName << " <path_to_image_or_directory>" << std::endl;
    std::cerr << "\nArguments:" << std::endl;
    std::cerr << "  path_to_image_or_directory   The full path to a single image file or a directory of images." << std::endl;
    std::cerr << "\nExample:" << std::endl;
    std::cerr << "  " << programName << " C:\\Users\\MyUser\\Pictures\\MyCat.jpg" << std::endl;
}


void printEffectiveConfiguration(const Config& config) {
    std::cout << "\n--- Effective Configuration ---" << std::endl;
    std::cout << "Target Width (Chars): " << config.targetWidth << std::endl;
    std::cout << "Aspect Correction:    " << config.charAspectRatioCorrection << std::endl;
    std::cout << "Font Path:            " << config.finalFontPath << std::endl;
    std::cout << "Font Size (PNG):      " << config.fontSize << "px" << std::endl;
    std::cout << "--- HTML Settings ---" << std::endl;
    std::cout << "Generate HTML Output: " << (config.generateHtmlOutput ? "Enabled" : "Disabled") << std::endl;
    std::cout << "HTML Font Size:       " << config.htmlFontSizePt << "pt" << std::endl;
    std::cout << "--- Schemes ---" << std::endl;
    std::cout << "Color Schemes:        ";
    if (config.schemesToGenerate.empty()) {
        std::cout << "(None - Check config)";
    } else {
        for (size_t i = 0; i < config.schemesToGenerate.size(); ++i) {
            std::cout << colorSchemeToString(config.schemesToGenerate[i]) << (i == config.schemesToGenerate.size() - 1 ? "" : ", ");
        }
    }
    std::cout << "\n-----------------------------" << std::endl;
}

void printProcessingSummary(int processedCount, int failedCount, double duration, const std::filesystem::path& outputDir) {
    std::cout << "\n==================================================" << std::endl;
    std::cout << "Processing Summary:" << std::endl;
    std::cout << "  Successfully processed: " << processedCount << " image(s)" << std::endl;
    std::cout << "  Failed/Skipped:       " << failedCount << " image(s)" << std::endl;
    std::cout << "  Total time:           " << std::fixed << std::setprecision(3) << duration << "s" << std::endl;
    if (!outputDir.empty()){
         std::cout << "Output(s) can be found in/under: " << outputDir.string() << std::endl;
    }
    std::cout << "==================================================" << std::endl;
}

} // namespace CLIHandler