#ifndef PNG_RENDERER_H
#define PNG_RENDERER_H

#include "IRenderer.h"

class PngRenderer : public IRenderer {
public:
    bool render(
        const std::vector<std::vector<CharColorInfo>>& asciiData,
        const std::filesystem::path& outputPath,
        const Config& config,
        ColorScheme scheme) const override;

    std::string getOutputFileExtension() const override;
};

#endif // PNG_RENDERER_H