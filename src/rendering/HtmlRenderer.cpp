#include "HtmlRenderer.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace { // Anonymous namespace for internal helpers

void setSchemeColors(ColorScheme scheme, unsigned char bgColor[3], unsigned char fgColor[3]) {
      switch (scheme) {
        case ColorScheme::AMBER_ON_BLACK:
            bgColor[0] = 0x00; bgColor[1] = 0x00; bgColor[2] = 0x00; fgColor[0] = 0xFF; fgColor[1] = 0xBF; fgColor[2] = 0x00; break;
        case ColorScheme::BLACK_ON_YELLOW:
            bgColor[0] = 0xFF; bgColor[1] = 0xFF; bgColor[2] = 0xAA; fgColor[0] = 0x00; fgColor[1] = 0x00; fgColor[2] = 0x00; break;
        case ColorScheme::BLACK_ON_CYAN:
            bgColor[0] = 0xAA; bgColor[1] = 0xFF; bgColor[2] = 0xFF; fgColor[0] = 0x00; fgColor[1] = 0x00; fgColor[2] = 0x00; break;
        case ColorScheme::COLOR_ON_WHITE:
            bgColor[0] = 0xC8; bgColor[1] = 0xC8; bgColor[2] = 0xC8; fgColor[0] = 0; fgColor[1] = 0; fgColor[2] = 0; break; // FG is per-char
        case ColorScheme::COLOR_ON_BLACK:
            bgColor[0] = 0x36; bgColor[1] = 0x36; bgColor[2] = 0x36; fgColor[0] = 0; fgColor[1] = 0; fgColor[2] = 0; break; // FG is per-char
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

// Helper to convert RGB to hex string for HTML
std::string rgbToHex(const unsigned char color[3]) {
    std::stringstream ss;
    ss << "#";
    for (int i = 0; i < 3; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(color[i]);
    }
    return ss.str();
}

// Helper to escape HTML special characters
std::string escapeHtmlChar(char c) {
    switch (c) {
        case '&':  return "&amp;";
        case '<':  return "&lt;";
        case '>':  return "&gt;";
        case '"':  return "&quot;";
        case '\'': return "&apos;";
        case ' ':  return "&nbsp;"; // Preserve multiple spaces
        default:   return std::string(1, c);
    }
}

} // end anonymous namespace

bool HtmlRenderer::render(
    const std::vector<std::vector<CharColorInfo>>& asciiData,
    const std::filesystem::path& outputPath,
    const Config& config,
    ColorScheme scheme) const
{
    if (asciiData.empty() || asciiData[0].empty()) {
        std::cerr << "Error: Cannot render empty ASCII data to HTML." << std::endl;
        return false;
    }

    std::ofstream htmlFile(outputPath);
    if (!htmlFile.is_open()) {
        std::cerr << "Error: Failed to open HTML file for writing: " << outputPath.string() << std::endl;
        return false;
    }

    std::cout << "Generating HTML: " << outputPath.filename().string() << std::endl;

    unsigned char schemeBgColor[3], schemeFgColor[3];
    setSchemeColors(scheme, schemeBgColor, schemeFgColor);
    bool usePixelColor = (scheme == ColorScheme::COLOR_ON_WHITE || scheme == ColorScheme::COLOR_ON_BLACK);

    std::string bodyBgColorHex = rgbToHex(schemeBgColor);
    std::string preFgColorHex = rgbToHex(schemeFgColor);

    // Extract font name from fontFilename for CSS
    std::filesystem::path fontPathObj(config.fontFilename);
    std::string cssFontFamily = fontPathObj.stem().string(); // Get "Consolas" from "Consolas.ttf"

    htmlFile << "<!DOCTYPE html>\n";
    htmlFile << "<html lang=\"en\">\n";
    htmlFile << "<head>\n";
    htmlFile << "  <meta charset=\"UTF-8\">\n";
    htmlFile << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    htmlFile << "  <title>ASCII Art</title>\n";
    htmlFile << "  <style>\n";
    htmlFile << "    body {\n";
    htmlFile << "      background-color: " << bodyBgColorHex << ";\n";
    htmlFile << "      color: " << preFgColorHex << ";\n"; // Default text color for body, can be overridden by pre
    htmlFile << "      margin: 0;\n";
    htmlFile << "      padding: 10px;\n";
    htmlFile << "    }\n";
    htmlFile << "    pre {\n";
    htmlFile << "      font-family: \"" << cssFontFamily << "\", Consolas, Menlo, Monaco, 'Courier New', monospace;\n";
    htmlFile << "      font-size: " << config.htmlFontSizePt << "pt;\n";
    htmlFile << "      line-height: 0.9em; /* Adjust for tighter packing if desired */\n"; // Smaller line-height can make it look more like a terminal
    htmlFile << "      white-space: pre;\n"; // Ensures spaces and line breaks are preserved
    if (!usePixelColor) { // Only set pre color if not using per-character colors extensively
        htmlFile << "      color: " << preFgColorHex << ";\n";
    }
    htmlFile << "      background-color: " << bodyBgColorHex << ";\n"; // pre should also have the scheme's BG
    htmlFile << "    }\n";
    htmlFile << "    span.char {\n";
    // If not using pixel color, the span color will be the scheme's foreground color
    // If using pixel color, this will be overridden by inline style
    htmlFile << "      color: " << preFgColorHex << ";\n";
    htmlFile << "    }\n";
    htmlFile << "  </style>\n";
    htmlFile << "</head>\n";
    htmlFile << "<body>\n";
    htmlFile << "<pre>";

    for (const auto& lineData : asciiData) {
        for (const auto& charInfo : lineData) {
            char c = charInfo.character;
            if (usePixelColor) {
                std::string charColorHex = rgbToHex(charInfo.color);
                htmlFile << "<span class=\"char\" style=\"color:" << charColorHex << ";\">";
                htmlFile << escapeHtmlChar(c);
                htmlFile << "</span>";
            } else {
                htmlFile << escapeHtmlChar(c);
            }
        }
        htmlFile << "\n"; // Newline for HTML <pre>
    }

    htmlFile << "</pre>\n";
    htmlFile << "</body>\n";
    htmlFile << "</html>\n";

    htmlFile.close();
    if (!htmlFile) {
        std::cerr << "Error: Failed to write all data or close the HTML file: " << outputPath.string() << std::endl;
        return false;
    }

    std::cout << "Successfully saved HTML to '" << outputPath.string() << "'" << std::endl;
    return true;
}

std::string HtmlRenderer::getOutputFileExtension() const {
    return ".html";
}