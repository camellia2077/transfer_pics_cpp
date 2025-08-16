// main.cpp
#define STB_IMAGE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stb/stb_image.h>
#include <stb/stb_truetype.h>
#include <stb/stb_image_write.h>

#include "common_types.h"     // Shared types (Config, ColorScheme, etc.)
#include "config_handler.h"   // Configuration loading/saving
#include "image_converter.h"  // Image to ASCII conversion function
#include "ascii_renderer.h"   // ASCII to Image/HTML rendering function



#include <iostream>
#include <string>
#include <vector>
#include <filesystem>   // For path operations (C++17)
#include <system_error> // For filesystem errors
#include <chrono>       // For timing
#include <iomanip>      // For setprecision
#include <memory>       // For unique_ptr
#include <optional>     // For return type of image_converter
#include <future>       // For std::async, std::future
#include <thread>       // For std::thread::hardware_concurrency
#include <functional>   // For std::cref
#include <limits>       // For numeric_limits

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;
using std::filesystem::path;
using namespace std::chrono;

path getExecutablePath(int argc, char* argv[]) {
     path exePath;
     try {
        if (argc > 0 && argv[0] != nullptr) {
             std::error_code ec;
             path tempPath = std::filesystem::canonical(argv[0], ec);
             if (!ec) {
                 exePath = tempPath;
             } else {
                 #ifdef _WIN32
                     char pathBuf[MAX_PATH];
                     if (GetModuleFileNameA(NULL, pathBuf, MAX_PATH) != 0) {
                         exePath = pathBuf;
                     } else {
                         exePath = std::filesystem::current_path() / "ascii_generator_fallback.exe";
                     }
                 #else
                      exePath = std::filesystem::current_path() / "ascii_generator_fallback";
                 #endif
             }
         } else {
             #ifdef _WIN32
                 char pathBuf[MAX_PATH];
                 if (GetModuleFileNameA(NULL, pathBuf, MAX_PATH) != 0) {
                     exePath = pathBuf;
                 } else {
                     exePath = std::filesystem::current_path() / "ascii_generator_fallback.exe";
                 }
             #else
                 exePath = std::filesystem::current_path() / "ascii_generator_fallback";
             #endif
         }

     } catch (const std::exception& e) {
         cerr << "Warning: Exception resolving executable path: " << e.what() << ". Using fallback." << endl;
         string fallbackName = "ascii_generator_fallback";
         #ifdef _WIN32
             fallbackName += ".exe";
         #endif
         exePath = std::filesystem::current_path() / fallbackName;
     }
     return exePath;
}


string getUserInputPath() {
    string inputPathStr;
    cout << "Please enter the image file or folder path to process: ";
    getline(std::cin, inputPathStr);
    inputPathStr.erase(0, inputPathStr.find_first_not_of(" \t\n\r\f\v\""));
    inputPathStr.erase(inputPathStr.find_last_not_of(" \t\n\r\f\v\"") + 1);
    if (inputPathStr.empty()) {
        cerr << "Error: Input path is empty." << endl;
    }
    return inputPathStr;
}

path setupOutputDirectory(const path& baseDir, const string& dirName) {
    path outputDirPath = baseDir / dirName;
    try {
        if (std::filesystem::create_directories(outputDirPath)) {
             cout << "Created output directory: " << outputDirPath.string() << endl;
        } else if (!std::filesystem::exists(outputDirPath) || !std::filesystem::is_directory(outputDirPath)) {
             cerr << "Error: Failed to create or access output directory: " << outputDirPath.string() << endl;
             return path(); 
        }
        return outputDirPath;
    } catch (const std::filesystem::filesystem_error& e) {
        cerr << "Error (filesystem): Creating directory " << outputDirPath.string() << ": " << e.what() << endl;
        return path(); 
    }
}


bool processImageFile(const path& imagePath,
                      const path& outputSubDirPath, 
                      const Config& config)
{
    cout << "\n==================================================" << endl;
    cout << "Processing IMAGE: " << imagePath.string() << endl;
    cout << "Output SubDir:  " << outputSubDirPath.string() << endl;
    cout << "==================================================" << endl;

    auto proc_start = high_resolution_clock::now();

    auto conversionResultOpt = convertImageToAscii(imagePath, config.targetWidth, config.charAspectRatioCorrection);

    if (!conversionResultOpt) {
        cerr << "-> Skipping image " << imagePath.filename().string() << " due to conversion failure." << endl;
        return false; 
    }

    const auto& conversionResult = *conversionResultOpt; 

    if (config.schemesToGenerate.empty()) {
        cerr << "Error: No color schemes configured to generate for " << imagePath.filename().string() << ". Skipping rendering." << endl;
        return false;
    }
     cout << "Processing " << config.schemesToGenerate.size() << " configured color scheme(s)..." << endl;

    bool allOutputsSuccessful = true; 
    for (const auto& currentScheme : config.schemesToGenerate) {
        string schemeSuffix = getSchemeSuffix(currentScheme); 
        string baseNameForOutput = imagePath.stem().string() + schemeSuffix;
        
        cout << "  Processing scheme: " << colorSchemeToString(currentScheme) << endl;

        // --- PNG Rendering ---
        string pngOutputFilename = baseNameForOutput + config.outputPngExtension;
        path finalPngOutputPath = outputSubDirPath / pngOutputFilename;
        cout << "    -> PNG: " << finalPngOutputPath.filename().string() << endl;

        bool pngRenderSuccess = renderAsciiToImage(
            conversionResult.data,       
            finalPngOutputPath,          
            config.finalFontPath,        
            config.fontSize,             
            currentScheme                
        );
        if (!pngRenderSuccess) {
            cerr << "    Error: Failed to render/save PNG for scheme " << colorSchemeToString(currentScheme) << "." << endl;
            allOutputsSuccessful = false; 
        }

        // --- HTML Rendering ---
        if (config.generateHtmlOutput) {
            string htmlOutputFilename = baseNameForOutput + config.outputHtmlExtension;
            path finalHtmlOutputPath = outputSubDirPath / htmlOutputFilename;
            cout << "    -> HTML: " << finalHtmlOutputPath.filename().string() << endl;

            bool htmlRenderSuccess = renderAsciiToHtml(
                conversionResult.data,
                finalHtmlOutputPath,
                config, // Pass the full config
                currentScheme
            );
            if (!htmlRenderSuccess) {
                cerr << "    Error: Failed to render/save HTML for scheme " << colorSchemeToString(currentScheme) << "." << endl;
                allOutputsSuccessful = false; 
            }
        }
    }

    auto proc_end = high_resolution_clock::now();
    cout << "-> Finished IMAGE processing '" << imagePath.filename().string() << "'. Time: "
         << std::fixed << std::setprecision(3) << duration_cast<milliseconds>(proc_end - proc_start).count() / 1000.0 << "s" << endl;

    return allOutputsSuccessful;
}


int main(int argc, char* argv[]) {
    cout << "--- ASCII Art Generator (Modular) ---" << endl;
    Config config; 

    path exePath = getExecutablePath(argc, argv);
    path exeDir = exePath.parent_path();
    // *** MODIFICATION: Change config filename to .json ***
    const string configFilename = "config.json";
    path configPathObj = exeDir / configFilename;

    if (!loadConfiguration(configPathObj, config)) {
         // A more severe message because JSON parsing failure is less recoverable
         cout << "Error: Configuration file could not be parsed correctly. Please check config.json. Proceeding with default values." << endl;
    }

    path potentialFontPath = exeDir / config.fontFilename;
     if (std::filesystem::exists(potentialFontPath) && std::filesystem::is_regular_file(potentialFontPath)) {
         config.finalFontPath = potentialFontPath.string();
     } else {
         potentialFontPath = std::filesystem::current_path() / config.fontFilename;
         if (std::filesystem::exists(potentialFontPath) && std::filesystem::is_regular_file(potentialFontPath)) {
             config.finalFontPath = potentialFontPath.string();
             cout << "Info: Font found in current working directory: " << config.finalFontPath << endl;
         } else {
             cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;
             cerr << "Error: Font file '" << config.fontFilename << "' not found!" << endl;
             cerr << "Searched near executable: " << (exeDir / config.fontFilename).string() << endl;
             cerr << "Searched in current dir: " << (std::filesystem::current_path() / config.fontFilename).string() << endl;
             cerr << "Please ensure '" << config.fontFilename << "' is placed correctly or update " << configFilename << "." << endl;
             cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;
             cout << "\nPress Enter to exit..." << endl;
             std::cin.clear();
             std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
             std::cin.get();
             return 1;
         }
     }

    cout << "\n--- Effective Configuration ---" << endl;
    cout << "Target Width (Chars): " << config.targetWidth << endl;
    cout << "Aspect Correction:    " << config.charAspectRatioCorrection << endl;
    cout << "Font Path:            " << config.finalFontPath << endl;
    cout << "Font Size (PNG):      " << config.fontSize << "px" << endl;
    cout << "Output Suffix (Img):  " << config.imageOutputSubDirSuffix << endl;
    cout << "Output Suffix (Batch):" << config.batchOutputSubDirSuffix << endl;
    cout << "--- HTML Settings ---" << endl;
    cout << "Generate HTML Output: " << (config.generateHtmlOutput ? "Enabled" : "Disabled") << endl;
    cout << "HTML Font Size:       " << config.htmlFontSizePt << "pt" << endl;
    cout << "HTML File Extension:  " << config.outputHtmlExtension << endl;
    cout << "--- Schemes ---" << endl;
    cout << "Color Schemes:        ";
    if (config.schemesToGenerate.empty()) {
        cout << "(None - Check config)";
    } else {
        for (size_t i = 0; i < config.schemesToGenerate.size(); ++i) {
            cout << colorSchemeToString(config.schemesToGenerate[i]) << (i == config.schemesToGenerate.size() - 1 ? "" : ", ");
        }
    }
    cout << "\n-----------------------------" << endl;

    string inputPathStr = getUserInputPath();
    if (inputPathStr.empty()) return 1;
    path inputPath(inputPathStr);

    int processedCount = 0;
    int failedCount = 0;
    auto overall_start_time = high_resolution_clock::now();
    path finalMainOutputDirPath; 

    try {
        if (!std::filesystem::exists(inputPath)) {
            cerr << "Error: Input path does not exist: " << inputPath.string() << endl;
            return 1;
        }

        if (std::filesystem::is_regular_file(inputPath)) {
            cout << "\nInput is a single file." << endl;
            if (isImageFile(inputPath)) { 
                 string imageSubDirName = inputPath.stem().string() + "_" + std::to_string(config.targetWidth) + config.imageOutputSubDirSuffix;
                 finalMainOutputDirPath = setupOutputDirectory(inputPath.parent_path(), imageSubDirName);

                 if (!finalMainOutputDirPath.empty()) {
                    path configOutputPath = finalMainOutputDirPath / "_run_config.txt";
                    if (!writeConfigToFile(config, configOutputPath)) {
                         cerr << "Warning: Failed to write configuration file for this run." << endl;
                    }
                    if (processImageFile(inputPath, finalMainOutputDirPath, config)) {
                        processedCount++;
                    } else {
                        failedCount++;
                    }
                 } else {
                    cerr << "Error: Failed to create output directory for " << inputPath.filename().string() << ". Skipping." << endl;
                    failedCount++;
                 }
            } else {
                cerr << "Error: Input file is not a supported image type: " << inputPath.string() << endl;
                failedCount++;
            }
        }
        else if (std::filesystem::is_directory(inputPath)) {
            cout << "\nInput is a directory. Processing images concurrently..." << endl;
             string batchDirName = inputPath.filename().string() + "_" + std::to_string(config.targetWidth) + config.batchOutputSubDirSuffix;
             finalMainOutputDirPath = setupOutputDirectory(inputPath.parent_path(), batchDirName);

             if (finalMainOutputDirPath.empty()) {
                 cerr << "Error: Failed to create main batch output directory. Aborting." << endl;
                 return 1;
             }

             path configOutputPath = finalMainOutputDirPath / "_run_config.txt";
             if (!writeConfigToFile(config, configOutputPath)) {
                  cerr << "Warning: Failed to write configuration file for this batch run." << endl;
             }

             vector<path> imageFilesToProcess;
             for (const auto& entry : std::filesystem::directory_iterator(inputPath)) {
                 if (entry.is_regular_file() && isImageFile(entry.path())) {
                     imageFilesToProcess.push_back(entry.path());
                 }
             }

             if (imageFilesToProcess.empty()) {
                 cout << "No supported image files found in directory: " << inputPath.string() << endl;
             } else {
                 cout << "Found " << imageFilesToProcess.size() << " image(s) to process." << endl;
                 vector<std::future<bool>> futures;
                 futures.reserve(imageFilesToProcess.size());

                 for(const auto& imgPath : imageFilesToProcess) {
                     string imageSubDirName = imgPath.stem().string() + "_" + std::to_string(config.targetWidth) + config.imageOutputSubDirSuffix;
                     path imageSpecificOutputDir = setupOutputDirectory(finalMainOutputDirPath, imageSubDirName);

                     if (!imageSpecificOutputDir.empty()) {
                         futures.push_back(
                             std::async(std::launch::async,
                                        processImageFile, 
                                        imgPath,               
                                        imageSpecificOutputDir, 
                                        std::cref(config))     
                         );
                     } else {
                         cerr << "Error: Failed to create output subdirectory for " << imgPath.filename().string() << " within batch. Skipping." << endl;
                         failedCount++; 
                     }
                 }

                 cout << "Waiting for processing tasks to complete..." << endl;
                 for (size_t i = 0; i < futures.size(); ++i) {
                     try {
                         if (futures[i].valid()) {
                             if (futures[i].get()) { 
                                 processedCount++;
                             } else {
                                 failedCount++; 
                             }
                         }
                     } catch (const std::exception& e) {
                         cerr << "Error retrieving result from processing task " << i << ": " << e.what() << endl;
                         failedCount++;
                     }
                 }
             }
        } else {
            cerr << "Error: Input path is not a file or directory: " << inputPath.string() << endl;
            return 1;
        }
    } catch (const std::exception& e) {
        cerr << "An unexpected error occurred during processing: " << e.what() << endl;
        cout << "\nPress Enter to exit..." << endl;
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cin.get();
        return 1;
    }

    auto overall_end_time = high_resolution_clock::now();
    cout << "\n==================================================" << endl;
    cout << "Processing Summary:" << endl;
    cout << "  Successfully processed: " << processedCount << " image(s)" << endl;
    cout << "  Failed/Skipped:       " << failedCount << " image(s)" << endl;
    cout << "  Total time:           " << std::fixed << std::setprecision(3)
         << duration_cast<duration<double>>(overall_end_time - overall_start_time).count() << "s" << endl;
    if (!finalMainOutputDirPath.empty()){
         cout << "Output(s) can be found in/under: " << finalMainOutputDirPath.string() << endl;
    }
    cout << "==================================================" << endl;

    cout << "\nPress Enter to exit..." << endl;
    std::cin.clear(); 
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    if (std::cin.peek() == EOF) { 
        std::cin.clear(); 
    }
    if (inputPathStr.find('\n') == string::npos && std::cin.peek() != '\n') {
    }
    std::cin.get(); 

    return (failedCount > 0); 
}
