#include "processing_orchestrator.h"
#include "conversion/image_converter.h"
#include "rendering/PngRenderer.h"
#include "rendering/HtmlRenderer.h"
#include "config/config_handler.h"
#include "utils/PathManager.h"

#include <iostream>
#include <future>
#include <chrono>
#include <iomanip>
#include <functional> // For std::cref

using namespace std::chrono;

ProcessingOrchestrator::ProcessingOrchestrator(const Config& config) : m_config(config) {
    setupRenderers();
}

void ProcessingOrchestrator::setupRenderers() {
    m_renderers.push_back(std::make_unique<PngRenderer>());
    if (m_config.generateHtmlOutput) {
        m_renderers.push_back(std::make_unique<HtmlRenderer>());
    }
}

void ProcessingOrchestrator::process(const std::filesystem::path& inputPath) {
    if (!std::filesystem::exists(inputPath)) {
        std::cerr << "Error: Input path does not exist: " << inputPath.string() << std::endl;
        m_failedCount++;
        return;
    }

    if (std::filesystem::is_regular_file(inputPath)) {
        processSingleImage(inputPath);
    } else if (std::filesystem::is_directory(inputPath)) {
        processDirectory(inputPath);
    } else {
        std::cerr << "Error: Input path is not a file or directory: " << inputPath.string() << std::endl;
        m_failedCount++;
    }
}

void ProcessingOrchestrator::processSingleImage(const std::filesystem::path& imagePath) {
    std::cout << "\nInput is a single file." << std::endl;
    if (isImageFile(imagePath)) {
        std::string imageSubDirName = imagePath.stem().string() + "_" + std::to_string(m_config.targetWidth) + m_config.imageOutputSubDirSuffix;
        m_finalMainOutputDirPath = PathManager::setupOutputDirectory(imagePath.parent_path(), imageSubDirName);

        if (!m_finalMainOutputDirPath.empty()) {
            std::filesystem::path configOutputPath = m_finalMainOutputDirPath / "_run_config.txt";
            if (!writeConfigToFile(m_config, configOutputPath)) {
                std::cerr << "Warning: Failed to write configuration file for this run." << std::endl;
            }
            if (processImageFile(imagePath, m_finalMainOutputDirPath)) {
                m_processedCount++;
            } else {
                m_failedCount++;
            }
        } else {
            std::cerr << "Error: Failed to create output directory for " << imagePath.filename().string() << ". Skipping." << std::endl;
            m_failedCount++;
        }
    } else {
        std::cerr << "Error: Input file is not a supported image type: " << imagePath.string() << std::endl;
        m_failedCount++;
    }
}

void ProcessingOrchestrator::processDirectory(const std::filesystem::path& dirPath) {
    std::cout << "\nInput is a directory. Processing images concurrently..." << std::endl;
    std::string batchDirName = dirPath.filename().string() + "_" + std::to_string(m_config.targetWidth) + m_config.batchOutputSubDirSuffix;
    m_finalMainOutputDirPath = PathManager::setupOutputDirectory(dirPath.parent_path(), batchDirName);

    if (m_finalMainOutputDirPath.empty()) {
        std::cerr << "Error: Failed to create main batch output directory. Aborting." << std::endl;
        return;
    }

    std::filesystem::path configOutputPath = m_finalMainOutputDirPath / "_run_config.txt";
    if (!writeConfigToFile(m_config, configOutputPath)) {
        std::cerr << "Warning: Failed to write configuration file for this batch run." << std::endl;
    }

    std::vector<std::filesystem::path> imageFilesToProcess;
    for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
        if (entry.is_regular_file() && isImageFile(entry.path())) {
            imageFilesToProcess.push_back(entry.path());
        }
    }

    if (imageFilesToProcess.empty()) {
        std::cout << "No supported image files found in directory: " << dirPath.string() << std::endl;
    } else {
        std::cout << "Found " << imageFilesToProcess.size() << " image(s) to process." << std::endl;
        std::vector<std::future<bool>> futures;
        futures.reserve(imageFilesToProcess.size());

        for(const auto& imgPath : imageFilesToProcess) {
            std::string imageSubDirName = imgPath.stem().string() + "_" + std::to_string(m_config.targetWidth) + m_config.imageOutputSubDirSuffix;
            std::filesystem::path imageSpecificOutputDir = PathManager::setupOutputDirectory(m_finalMainOutputDirPath, imageSubDirName);

            if (!imageSpecificOutputDir.empty()) {
                futures.push_back(
                    std::async(std::launch::async,
                               &ProcessingOrchestrator::processImageFile, this,
                               imgPath,
                               imageSpecificOutputDir)
                );
            } else {
                std::cerr << "Error: Failed to create output subdirectory for " << imgPath.filename().string() << " within batch. Skipping." << std::endl;
                m_failedCount++;
            }
        }

        std::cout << "Waiting for processing tasks to complete..." << std::endl;
        for (size_t i = 0; i < futures.size(); ++i) {
            try {
                if (futures[i].valid()) {
                    if (futures[i].get()) {
                        m_processedCount++;
                    } else {
                        m_failedCount++;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error retrieving result from processing task " << i << ": " << e.what() << std::endl;
                m_failedCount++;
            }
        }
    }
}


bool ProcessingOrchestrator::processImageFile(const std::filesystem::path& imagePath, const std::filesystem::path& outputSubDirPath) {
    std::cout << "\n==================================================" << std::endl;
    std::cout << "Processing IMAGE: " << imagePath.string() << std::endl;
    std::cout << "Output SubDir:  " << outputSubDirPath.string() << std::endl;
    std::cout << "==================================================" << std::endl;

    auto proc_start = high_resolution_clock::now();

    auto conversionResultOpt = convertImageToAscii(imagePath, m_config.targetWidth, m_config.charAspectRatioCorrection);

    if (!conversionResultOpt) {
        std::cerr << "-> Skipping image " << imagePath.filename().string() << " due to conversion failure." << std::endl;
        return false;
    }

    const auto& conversionResult = *conversionResultOpt;

    if (m_config.schemesToGenerate.empty()) {
        std::cerr << "Error: No color schemes configured to generate for " << imagePath.filename().string() << ". Skipping rendering." << std::endl;
        return false;
    }
    std::cout << "Processing " << m_config.schemesToGenerate.size() << " configured color scheme(s)..." << std::endl;

    bool allOutputsSuccessful = true;
    for (const auto& currentScheme : m_config.schemesToGenerate) {
        std::string schemeSuffix = getSchemeSuffix(currentScheme);
        std::string baseNameForOutput = imagePath.stem().string() + schemeSuffix;

        std::cout << "  Processing scheme: " << colorSchemeToString(currentScheme) << std::endl;

        for (const auto& renderer : m_renderers) {
            std::string outputFilename = baseNameForOutput + renderer->getOutputFileExtension();
            std::filesystem::path finalOutputPath = outputSubDirPath / outputFilename;

            std::cout << "    -> " << renderer->getOutputFileExtension().substr(1) << ": " << finalOutputPath.filename().string() << std::endl;

            if (!renderer->render(conversionResult.data, finalOutputPath, m_config, currentScheme)) {
                std::cerr << "    Error: Failed to render/save " << renderer->getOutputFileExtension() << " for scheme " << colorSchemeToString(currentScheme) << "." << std::endl;
                allOutputsSuccessful = false;
            }
        }
    }

    auto proc_end = high_resolution_clock::now();
    std::cout << "-> Finished IMAGE processing '" << imagePath.filename().string() << "'. Time: "
         << std::fixed << std::setprecision(3) << duration_cast<milliseconds>(proc_end - proc_start).count() / 1000.0 << "s" << std::endl;

    return allOutputsSuccessful;
}