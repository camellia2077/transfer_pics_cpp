#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <stdexcept>
#include <cstdlib>
#include <sstream>
#include <filesystem> 
#include <set>        

// Define these macros to tell stb libraries to implement functions here
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// *** Include and implement stb_image_resize2 ***
#define STB_IMAGE_RESIZE_IMPLEMENTATION // Or STB_IMAGE_RESIZE2_IMPLEMENTATION
#include "stb_image_resize2.h" // Ensure filename is correct

#ifdef _WIN32
#include <windows.h> // For SetConsoleOutputCP (optional, for console display)
#endif

namespace fs = std::filesystem; // Alias for convenience
using namespace std;

// --- Forward Declarations ---
bool getUserInput(fs::path& input_path, int& pixel_size);
fs::path createOutputDirectory(const fs::path& input_dir);
unsigned char* loadImage(const fs::path& filepath, int& width, int& height, int& channels);
void pixelateImage(const unsigned char* input_data, int width, int height, int channels, int pixel_size, unsigned char* output_data);
void saveImage(const fs::path& filepath, int width, int height, int channels, const unsigned char* data);
bool processImageFile(const fs::path& input_file_path, const fs::path& output_dir, int pixel_size, const string& suffix, const string& output_extension);
stbir_pixel_layout get_pixel_layout(int channels);
void* safe_malloc(size_t size);

// --- Helper Functions (Unchanged) ---

// Helper function: Safe memory allocation
void* safe_malloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr && size > 0) {
        throw std::runtime_error("Memory allocation failed (malloc)");
    }
    return ptr;
}

// Helper function: Get Pixel Layout based on channels
stbir_pixel_layout get_pixel_layout(int channels) {
    switch (channels) {
        case 1: return STBIR_1CHANNEL;
        case 2: return STBIR_2CHANNEL;
        case 3: return STBIR_RGB;      // Assuming RGB order
        case 4: return STBIR_RGBA;     // Assuming RGBA order
        default:
            throw std::runtime_error("Unsupported number of channels: " + std::to_string(channels));
    }
    // Note: Adjust if your input is BGR, etc., or needs premultiplied alpha (STBIR_RGBA_PM)
}

// --- Modified and New Functions ---

/**
 * @brief Gets user input for a folder path and block size.
 * @param input_path Reference to store the input folder path.
 * @param pixel_size Reference to store the input block size.
 * @return true if input is syntactically valid, false otherwise.
 */
bool getUserInput(fs::path& input_path, int& pixel_size) {
    string path_str;
    cout << "Input FOLDER path: ";
    // Use getline to handle paths with spaces
    getline(cin, path_str);
    // Trim leading/trailing whitespace (optional but good practice)
    path_str.erase(0, path_str.find_first_not_of(" \t\n\r\f\v\""));
    path_str.erase(path_str.find_last_not_of(" \t\n\r\f\v\"") + 1);

    try {
        input_path = fs::path(path_str);
    } catch (const std::exception& e) {
        cerr << "Error: Invalid path format '" << path_str << "'. " << e.what() << endl;
        return false;
    }

    cout << "Input block size (e.g., 8, 16): ";
    if (!(cin >> pixel_size)) {
        cerr << "Error: Invalid input for block size. Please enter an integer." << endl;
        cin.clear(); // Clear error flags
        cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Discard bad input
        return false;
    }
    // Consume the remaining newline character after reading the integer
    cin.ignore(numeric_limits<streamsize>::max(), '\n');


    if (pixel_size <= 0) {
        cerr << "Error: Block size must be a positive integer." << endl;
        return false;
    }
    return true;
}

/**
 * @brief Creates an output directory named "pixelated_output" inside the input directory.
 * @param input_dir The path to the input directory.
 * @return The path to the created (or existing) output directory.
 * @throws std::runtime_error if the output directory cannot be created.
 */
fs::path createOutputDirectory(const fs::path& input_dir) {
    fs::path output_dir = input_dir / "pixelated_output";
    try {
        if (!fs::exists(output_dir)) {
            if (fs::create_directory(output_dir)) {
                cout << "Created output directory: " << output_dir.string() << endl;
            } else {
                throw std::runtime_error("Failed to create output directory (unknown reason): " + output_dir.string());
            }
        } else if (!fs::is_directory(output_dir)) {
            throw std::runtime_error("Output path exists but is not a directory: " + output_dir.string());
        } else {
             cout << "Output directory already exists: " << output_dir.string() << endl;
        }
    } catch (const fs::filesystem_error& e) {
        throw std::runtime_error("Filesystem error creating output directory: " + output_dir.string() + " - " + e.what());
    }
    return output_dir;
}


/**
 * @brief Loads an image from the specified file path.
 * @param filepath Path to the image file.
 * @param width Reference to store the image width.
 * @param height Reference to store the image height.
 * @param channels Reference to store the number of color channels.
 * @return Pointer to the loaded image data. Throws std::runtime_error on failure.
 * @note The caller is responsible for freeing the returned pointer using stbi_image_free().
 */
unsigned char* loadImage(const fs::path& filepath, int& width, int& height, int& channels) {
    // Use filepath.string() or filepath.u8string() for compatibility
    // u8string is generally safer for non-ASCII paths, especially on Windows
    #ifdef _WIN32
        std::string path_str = filepath.string(); // Or use WideCharToMultiByte if needed for older APIs
    #else
        std::string path_str = filepath.string();
    #endif

    cout << "Loading image '" << path_str << "'..." << endl;

    unsigned char* data = stbi_load(path_str.c_str(), &width, &height, &channels, 0);
    if (!data) {
        std::stringstream ss;
        ss << "Error: Cannot load image " << path_str << ". STB Error: " << (stbi_failure_reason() ? stbi_failure_reason() : "Unknown error");
        throw std::runtime_error(ss.str());
    }
    cout << " -> Loaded: " << width << "x" << height << ", " << channels << " channels" << std::endl;
    return data;
}

/**
 * @brief Pixelates an image using the resize method (shrink then enlarge).
 * @param input_data Pointer to the original image data.
 * @param width Width of the original image.
 * @param height Height of the original image.
 * @param channels Number of color channels.
 * @param pixel_size The target size of the pixel blocks.
 * @param output_data Pointer to the pre-allocated buffer for the pixelated image.
 * @throws std::runtime_error If resizing fails or arguments are invalid.
 */
void pixelateImage(const unsigned char* input_data, int width, int height, int channels, int pixel_size, unsigned char* output_data) {
     if (!input_data || !output_data || width <= 0 || height <= 0 || channels <= 0 || pixel_size <= 0) {
         throw std::runtime_error("Invalid arguments passed to pixelateImage.");
     }

     // --- Calculate dimensions for the small intermediate image ---
     int small_width = std::max(1, static_cast<int>(std::round(static_cast<double>(width) / pixel_size)));
     int small_height = std::max(1, static_cast<int>(std::round(static_cast<double>(height) / pixel_size)));
     // cout << "Target small dimensions: " << small_width << "x" << small_height << endl; // Debug

     // --- Allocate memory for the small intermediate image ---
     size_t small_image_size = static_cast<size_t>(small_width) * small_height * channels;
     unsigned char* small_image_data = nullptr; // Initialize to nullptr
     try {
         small_image_data = static_cast<unsigned char*>(safe_malloc(small_image_size));

         stbir_pixel_layout layout = get_pixel_layout(channels); // Get layout once

         // --- 1. Shrink image (using POINT_SAMPLE) ---
         // cout << "Calling stbir_resize (shrink)..." << endl; // Debug
         void* resize_result_shrink_ptr = stbir_resize(
             input_data, width, height, width * channels,             // Input
             small_image_data, small_width, small_height, small_width * channels, // Output
             layout,                  // Pixel Layout
             STBIR_TYPE_UINT8,        // Datatype
             STBIR_EDGE_CLAMP,        // Edge Mode
             STBIR_FILTER_POINT_SAMPLE  // Filter (Nearest Neighbor)
         );
         if (!resize_result_shrink_ptr) {
             throw std::runtime_error("Error: Failed to shrink image using stbir_resize.");
         }
         // cout << "Image shrink successful." << endl; // Debug

         // --- 2. Enlarge image back to original size (using POINT_SAMPLE) ---
         // cout << "Calling stbir_resize (enlarge)..." << endl; // Debug
         void* resize_result_enlarge_ptr = stbir_resize(
             small_image_data, small_width, small_height, small_width * channels, // Input
             output_data, width, height, width * channels,         // Output (pre-allocated by caller)
             layout,                  // Pixel Layout
             STBIR_TYPE_UINT8,        // Datatype
             STBIR_EDGE_CLAMP,        // Edge Mode
             STBIR_FILTER_POINT_SAMPLE  // Filter (Nearest Neighbor)
         );
         if (!resize_result_enlarge_ptr) {
             throw std::runtime_error("Error: Failed to enlarge image using stbir_resize.");
         }
         // cout << "Image enlarge successful." << endl; // Debug

         // --- Free the intermediate small image data ---
         free(small_image_data); // Free the temporary buffer
         small_image_data = nullptr; // Avoid dangling pointer issues

     } catch (...) {
         // Ensure intermediate buffer is freed even if an error occurs after allocation
         free(small_image_data);
         // Re-throw the caught exception
         throw;
     }
}


/**
 * @brief Saves image data to a PNG file.
 * @param filepath The path (including filename) to save the image to.
 * @param width Width of the image.
 * @param height Height of the image.
 * @param channels Number of color channels.
 * @param data Pointer to the image data to save.
 * @throws std::runtime_error If writing the image fails or data is null.
 */
void saveImage(const fs::path& filepath, int width, int height, int channels, const unsigned char* data) {
    if (!data) {
        throw std::runtime_error("Error: Cannot save null image data.");
    }
     // Use filepath.string() or filepath.u8string()
    #ifdef _WIN32
        std::string path_str = filepath.string();
    #else
        std::string path_str = filepath.string();
    #endif

    cout << " -> Saving image to: " << path_str << endl;
    int write_result = stbi_write_png(path_str.c_str(), width, height, channels, data, width * channels);
    if (!write_result) {
        throw std::runtime_error("Error: Cannot write image to " + path_str);
    }
    cout << " -> Pixelated image saved successfully." << std::endl;
}

/**
 * @brief Processes a single image file: loads, pixelates, and saves it.
 * @param input_file_path Path to the input image file.
 * @param output_dir Path to the directory where the output image should be saved.
 * @param pixel_size The desired pixel block size.
 * @param suffix Suffix to add to the output filename (before extension).
 * @param output_extension The desired output file extension (e.g., ".png").
 * @return true if processing was successful, false otherwise.
 */
bool processImageFile(const fs::path& input_file_path, const fs::path& output_dir, int pixel_size, const string& suffix, const string& output_extension) {
    int width = 0, height = 0, channels = 0;
    unsigned char* input_image_data = nullptr;
    unsigned char* output_image_data = nullptr;
    bool success = false;

    try {
        // --- Load Input Image ---
        input_image_data = loadImage(input_file_path, width, height, channels);

        // --- Generate Output Filename ---
        fs::path stem = input_file_path.stem(); // Filename without extension
        fs::path output_filename = stem.string() + suffix + output_extension;
        fs::path output_file_path = output_dir / output_filename;

        // --- Allocate Memory for Output Image ---
        size_t output_image_size = static_cast<size_t>(width) * height * channels;
        output_image_data = static_cast<unsigned char*>(safe_malloc(output_image_size));

        // --- Perform Pixelation ---
        pixelateImage(input_image_data, width, height, channels, pixel_size, output_image_data);

        // --- Save Processed Image ---
        saveImage(output_file_path, width, height, channels, output_image_data);

        success = true;

    } catch (const std::exception& e) {
        cerr << " !! Error processing file '" << input_file_path.string() << "': " << e.what() << endl;
        success = false;
    }

    // --- Free all allocated memory for this file ---
    stbi_image_free(input_image_data); // Safe to call on nullptr
    free(output_image_data);           // Safe to call on nullptr

    return success;
}


// --- Main Function (Orchestrator) ---
int main() {
#ifdef _WIN32
    // Optional: Set console to UTF-8
    // SetConsoleOutputCP(CP_UTF8);
    // SetConsoleCP(CP_UTF8);
#endif

    fs::path input_path;
    int pixel_size = 0;
    fs::path output_dir;

    // --- 1. Get User Input ---
    if (!getUserInput(input_path, pixel_size)) {
        return 1; // Exit if input is invalid
    }

    // --- 2. Validate Input Path as Directory ---
    std::error_code ec; // To check filesystem errors without exceptions
    if (!fs::exists(input_path, ec) || ec) {
         cerr << "Error: Input path does not exist or cannot be accessed: " << input_path.string() << (ec ? " (" + ec.message() + ")" : "") << endl;
         return 1;
    }
     if (!fs::is_directory(input_path, ec) || ec) {
         cerr << "Error: Input path is not a directory: " << input_path.string() << (ec ? " (" + ec.message() + ")" : "") << endl;
         return 1;
     }

    // --- 3. Create Output Directory ---
    try {
        output_dir = createOutputDirectory(input_path);
    } catch (const std::exception& e) {
        cerr << "Error creating output directory: " << e.what() << endl;
        return 1;
    }

    // --- Timing variables for the whole process ---
    auto overall_start_time = std::chrono::high_resolution_clock::now();

    // --- Define supported image extensions (case-insensitive) ---
    const std::set<std::string> image_extensions = {
        ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif"
        // Add more if needed
    };

    // --- Counters ---
    int files_processed = 0;
    int files_succeeded = 0;
    int files_failed = 0;
    int files_skipped = 0;

    cout << "\nProcessing files in directory: " << input_path.string() << endl;
    cout << "-------------------------------------------" << endl;

    // --- 4. Iterate through directory and process images ---
    try {
        for (const auto& entry : fs::directory_iterator(input_path)) {
            const auto& current_path = entry.path();
             std::error_code entry_ec;
            if (fs::is_regular_file(entry.status(entry_ec)) && !entry_ec) { // Check if it's a regular file
                string ext = current_path.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower); // Convert extension to lowercase

                if (image_extensions.count(ext)) { // Check if the extension is supported
                    files_processed++;
                    cout << "\n[" << files_processed << "] Processing: " << current_path.filename().string() << endl;
                    if (processImageFile(current_path, output_dir, pixel_size, "_pixelated_resize", ".png")) {
                        files_succeeded++;
                    } else {
                        files_failed++;
                    }
                } else {
                     // Optional: Log skipped files
                     // cout << "Skipping non-image file: " << current_path.filename().string() << endl;
                    files_skipped++;
                }
            } else if(entry_ec) {
                 cerr << " !! Warning: Could not get status for '" << current_path.string() << "': " << entry_ec.message() << endl;
                 files_skipped++;
            }
            // Optional: Handle subdirectories if needed (e.g., using recursive_directory_iterator)
        }
    } catch (const fs::filesystem_error& e) {
        cerr << "\n !! Filesystem error during directory iteration: " << e.what() << endl;
        // Continue to summary if possible
    } catch (const std::exception& e) {
         cerr << "\n !! An unexpected error occurred during processing: " << e.what() << endl;
         // Continue to summary if possible
    }


    // --- 5. Output Summary and Timing ---
    auto overall_end_time = std::chrono::high_resolution_clock::now();
    chrono::duration<double, std::milli> duration_ms = overall_end_time - overall_start_time;

    cout << "\n-------------------------------------------" << endl;
    cout << "Processing Summary:" << endl;
    cout << " - Total files encountered in directory: " << (files_processed + files_skipped) << endl;
    cout << " - Image files processed:              " << files_processed << endl;
    cout << " - Files skipped (non-image/error):    " << files_skipped << endl;
    cout << " - Successfully pixelated:           " << files_succeeded << endl;
    cout << " - Failed to pixelate:               " << files_failed << endl;
    cout << "-------------------------------------------" << endl;
    cout << "Total Processing Time:" << endl;
    cout << std::fixed << std::setprecision(3)
         << duration_ms.count() / 1000.0 << " seconds ("
         << duration_ms.count() << " ms)" << std::endl;
    cout << "-------------------------------------------" << endl;
    cout << "Output saved to: " << output_dir.string() << endl;
    cout << "-------------------------------------------" << endl;

    return (files_failed == 0 && files_processed > 0) ? 0 : 1; // Return 0 if all attempted images succeeded, 1 otherwise
}
