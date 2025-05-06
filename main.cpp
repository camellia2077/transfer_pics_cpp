// main.cpp
#include "common_types.h"     // Shared types (Config, ColorScheme, etc.)
#include "config_handler.h"   // Configuration loading/saving
#include "image_converter.h"  // Image to ASCII conversion function
#include "ascii_renderer.h"   // ASCII to Image rendering function

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>   // For path operations (C++17)
#include <system_error> // For filesystem errors
#include <chrono>       // For timing
#include <iomanip>      // For setprecision
#include <memory>       // For unique_ptr (though less needed now)
#include <optional>     // For return type of image_converter
#include <future>       // For std::async, std::future
#include <thread>       // For std::thread::hardware_concurrency
#include <functional>   // For std::cref
#include <limits>       // For numeric_limits

// ADD THIS BLOCK FOR WINDOWS API (if needed for exe path)
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

// --- Helper Functions specific to main orchestration ---

// Gets the executable path (same logic as original)
path getExecutablePath(int argc, char* argv[]) {
     path exePath;
     try {
         // (Same logic as original main to find exe path using argv[0], GetModuleFileNameA, etc.)
        // ... [Copy the original logic for finding exePath here] ...
        // Simplified fallback:
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
             // Fallback if argv[0] is not available
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
    // Trim whitespace (optional but good practice)
    inputPathStr.erase(0, inputPathStr.find_first_not_of(" \t\n\r\f\v\""));
    inputPathStr.erase(inputPathStr.find_last_not_of(" \t\n\r\f\v\"") + 1);
    if (inputPathStr.empty()) {
        cerr << "Error: Input path is empty." << endl;
    }
    return inputPathStr;
}

// Creates the specific output subdirectory for an image (or the main batch dir)
// Returns the created path, or empty path on error.
path setupOutputDirectory(const path& baseDir, const string& dirName) {
    path outputDirPath = baseDir / dirName;
    try {
        if (std::filesystem::create_directories(outputDirPath)) {
             cout << "Created output directory: " << outputDirPath.string() << endl;
        } else if (!std::filesystem::exists(outputDirPath) || !std::filesystem::is_directory(outputDirPath)) {
             cerr << "Error: Failed to create or access output directory: " << outputDirPath.string() << endl;
             return path(); // Return empty path on error
        }
        return outputDirPath;
    } catch (const std::filesystem::filesystem_error& e) {
        cerr << "Error (filesystem): Creating directory " << outputDirPath.string() << ": " << e.what() << endl;
        return path(); // Return empty path on error
    }
}


// --- Core Processing Function (for one image) ---
// This function now coordinates conversion and rendering for a single image file.
bool processImageFile(const path& imagePath,
                      const path& outputSubDirPath, // The final specific directory for this image's outputs
                      const Config& config)
{
    cout << "\n==================================================" << endl;
    cout << "Processing IMAGE: " << imagePath.string() << endl;
    cout << "Output SubDir:  " << outputSubDirPath.string() << endl;
    cout << "==================================================" << endl;

    auto proc_start = high_resolution_clock::now();

    // 1. Convert Image to ASCII data
    auto conversionResultOpt = convertImageToAscii(imagePath, config.targetWidth, config.charAspectRatioCorrection);

    if (!conversionResultOpt) {
        cerr << "-> Skipping image " << imagePath.filename().string() << " due to conversion failure." << endl;
        return false; // Conversion failed
    }

    const auto& conversionResult = *conversionResultOpt; // Get the result data

    // 2. Check if any schemes are configured
    if (config.schemesToGenerate.empty()) {
        cerr << "Error: No color schemes configured to generate for " << imagePath.filename().string() << ". Skipping rendering." << endl;
        // Conversion succeeded, but nothing to render. Count as success or failure? Let's say failure as no output generated.
        return false;
    }
     cout << "Processing " << config.schemesToGenerate.size() << " configured color scheme(s)..." << endl;


    // 3. Render ASCII data for each configured scheme
    bool allSchemesSuccessful = true;
    for (const auto& currentScheme : config.schemesToGenerate) {
        string schemeSuffix = getSchemeSuffix(currentScheme); // Helper from common_types/config_handler
        string baseNameForOutput = imagePath.stem().string() + schemeSuffix;
        string pngOutputFilename = baseNameForOutput + config.outputPngExtension;
        path finalPngOutputPath = outputSubDirPath / pngOutputFilename;

        cout << "  Processing scheme: " << colorSchemeToString(currentScheme)
             << " -> PNG: " << finalPngOutputPath.filename().string() << endl;

        // Call the renderer module
        bool renderSuccess = renderAsciiToImage(
            conversionResult.data,       // The ASCII data
            finalPngOutputPath,          // Where to save the PNG
            config.finalFontPath,        // Resolved font path
            config.fontSize,             // Font size from config
            currentScheme                // The scheme to render
        );

        if (!renderSuccess) {
            cerr << "  Error: Failed to render/save PNG for scheme " << colorSchemeToString(currentScheme) << "." << endl;
            allSchemesSuccessful = false; // Mark failure for this image if any scheme fails
        }
    }

    auto proc_end = high_resolution_clock::now();
    cout << "-> Finished IMAGE processing '" << imagePath.filename().string() << "'. Time: "
         << std::fixed << std::setprecision(3) << duration_cast<milliseconds>(proc_end - proc_start).count() / 1000.0 << "s" << endl;

    return allSchemesSuccessful;
}


// --- Main Function ---
int main(int argc, char* argv[]) {
    cout << "--- ASCII Art Generator (Modular) ---" << endl;
    Config config; // Initialize with defaults

    // 1. Determine Paths (Executable, Config)
    path exePath = getExecutablePath(argc, argv);
    path exeDir = exePath.parent_path();
    const string configFilename = "config.ini";
    path configPathObj = exeDir / configFilename;

    // 2. Load Configuration
    if (!loadConfiguration(configPathObj, config)) {
         // A warning was already printed by loadConfiguration about parsing errors
         // Continue with potentially partially loaded config / defaults
         cout << "Warning: Configuration file had parsing issues. Proceeding with loaded/default values." << endl;
    }

    // 3. Resolve Final Font Path (relative to exe dir first, then current dir)
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

             // Wait for user before exiting
             cout << "\nPress Enter to exit..." << endl;
             std::cin.clear();
             std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
             std::cin.get();
             return 1;
         }
     }


    // 4. Print Configuration Summary
    cout << "\n--- Effective Configuration ---" << endl;
    cout << "Target Width (Chars): " << config.targetWidth << endl;
    cout << "Aspect Correction:    " << config.charAspectRatioCorrection << endl;
    cout << "Font Path:            " << config.finalFontPath << endl;
    cout << "Font Size (PNG):      " << config.fontSize << "px" << endl;
    // cout << "Tiled Rendering:      " << (config.enableTiledRendering ? "Enabled" : "Disabled") << " (Tile Size: " << config.tileSize << ")" << endl; // Tiling not implemented in renderer yet
    cout << "Output Suffix (Img):  " << config.imageOutputSubDirSuffix << endl;
    cout << "Output Suffix (Batch):" << config.batchOutputSubDirSuffix << endl;
    cout << "Color Schemes:        ";
    if (config.schemesToGenerate.empty()) {
        cout << "(None - Check config)";
    } else {
        for (size_t i = 0; i < config.schemesToGenerate.size(); ++i) {
            cout << colorSchemeToString(config.schemesToGenerate[i]) << (i == config.schemesToGenerate.size() - 1 ? "" : ", ");
        }
    }
    cout << "\n-----------------------------" << endl;

    // 5. Get User Input Path
    string inputPathStr = getUserInputPath();
    if (inputPathStr.empty()) return 1;
    path inputPath(inputPathStr);

    // 6. Process Input (File or Directory)
    int processedCount = 0;
    int failedCount = 0;
    auto overall_start_time = high_resolution_clock::now();
    path finalMainOutputDirPath; // Common output dir for config (batch dir or single image dir)

    try {
        if (!std::filesystem::exists(inputPath)) {
            cerr << "Error: Input path does not exist: " << inputPath.string() << endl;
            return 1;
        }

        // --- Single File Processing ---
        if (std::filesystem::is_regular_file(inputPath)) {
            cout << "\nInput is a single file." << endl;
            if (isImageFile(inputPath)) { // Use helper from common_types
                 // 1. Create the output subdirectory for this single file
                 string imageSubDirName = inputPath.stem().string() + "_" + std::to_string(config.targetWidth) + config.imageOutputSubDirSuffix;
                 finalMainOutputDirPath = setupOutputDirectory(inputPath.parent_path(), imageSubDirName);

                 if (!finalMainOutputDirPath.empty()) {
                    // 2. Write config into this directory
                    path configOutputPath = finalMainOutputDirPath / "_run_config.txt";
                    if (!writeConfigToFile(config, configOutputPath)) {
                         cerr << "Warning: Failed to write configuration file for this run." << endl;
                    }
                    // 3. Process the single image
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
        // --- Directory Processing ---
        else if (std::filesystem::is_directory(inputPath)) {
            cout << "\nInput is a directory. Processing images concurrently..." << endl;

             // 1. Create the main batch output directory
             string batchDirName = inputPath.filename().string() + "_" + std::to_string(config.targetWidth) + config.batchOutputSubDirSuffix + "_PNG";
             finalMainOutputDirPath = setupOutputDirectory(inputPath.parent_path(), batchDirName);

             if (finalMainOutputDirPath.empty()) {
                 cerr << "Error: Failed to create main batch output directory. Aborting." << endl;
                 return 1;
             }

             // 2. Write config into the main batch directory
             path configOutputPath = finalMainOutputDirPath / "_run_config.txt";
             if (!writeConfigToFile(config, configOutputPath)) {
                  cerr << "Warning: Failed to write configuration file for this batch run." << endl;
             }

             // 3. Find images and prepare tasks
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

                 // 4. Launch tasks for each image
                 for(const auto& imgPath : imageFilesToProcess) {
                     // Create the specific subdirectory *inside* the main batch dir for this image
                     string imageSubDirName = imgPath.stem().string() + "_" + std::to_string(config.targetWidth) + config.imageOutputSubDirSuffix;
                     path imageSpecificOutputDir = setupOutputDirectory(finalMainOutputDirPath, imageSubDirName);

                     if (!imageSpecificOutputDir.empty()) {
                         // Launch async task, passing the image's specific output dir
                         futures.push_back(
                             std::async(std::launch::async,
                                        processImageFile, // The function to call
                                        imgPath,               // Argument 1: image path
                                        imageSpecificOutputDir, // Argument 2: specific output dir
                                        std::cref(config))     // Argument 3: config (by const ref)
                         );
                     } else {
                         cerr << "Error: Failed to create output subdirectory for " << imgPath.filename().string() << " within batch. Skipping." << endl;
                         failedCount++; // Count as failed if dir creation fails
                     }
                 }

                 // 5. Wait for tasks and collect results
                 cout << "Waiting for processing tasks to complete..." << endl;
                 for (size_t i = 0; i < futures.size(); ++i) {
                     try {
                         if (futures[i].valid()) {
                             if (futures[i].get()) { // .get() waits and returns the bool result
                                 processedCount++;
                             } else {
                                 failedCount++; // The task reported failure
                             }
                         }
                         // If future wasn't valid, it's likely counted in failedCount already (dir creation failure)
                     } catch (const std::exception& e) {
                         // This might catch exceptions thrown *within* processImageFile if not caught there
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
        // Wait for user before exiting
        cout << "\nPress Enter to exit..." << endl;
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cin.get();
        return 1;
    }

    // 7. Print Summary
    auto overall_end_time = high_resolution_clock::now();
    cout << "\n==================================================" << endl;
    cout << "Processing Summary:" << endl;
    cout << "  Successfully processed: " << processedCount << " image(s)" << endl;
    cout << "  Failed/Skipped:       " << failedCount << " image(s)" << endl;
    cout << "  Total time:           " << std::fixed << std::setprecision(3)
         << duration_cast<duration<double>>(overall_end_time - overall_start_time).count() << "s" << endl;
    cout << "==================================================" << endl;


    // 8. Wait for user before exiting
    cout << "\nPress Enter to exit..." << endl;
    std::cin.clear(); // Clear potential error flags on cin
    // Discard any characters currently in the input buffer up to the next newline
    // This is important if the user accidentally typed something before pressing Enter
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    if (std::cin.peek() == EOF) { // Handle case where input stream is closed
        std::cin.clear(); // Clear EOF flag
    }
     // If previous input was getline, an extra ignore might be needed depending on platform/previous state
    if (inputPathStr.find('\n') == string::npos && std::cin.peek() != '\n') {
         // If getline didn't consume a newline and buffer doesn't start with one
    }
    std::cin.get(); // Wait for Enter key press


    return (failedCount > 0); // Return 0 on success (no failures), non-zero if failures occurred
}