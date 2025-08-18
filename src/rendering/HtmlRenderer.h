#ifndef HTML_RENDERER_H
#define HTML_RENDERER_H

#include "IRenderer.h"

class HtmlRenderer : public IRenderer {
public:
    bool render(
        const std::vector<std::vector<CharColorInfo>>& asciiData,
        const std::filesystem::path& outputPath,
        const Config& config,
        ColorScheme scheme) const override;

    std::string getOutputFileExtension() const override;
};

#endif // HTML_RENDERER_H