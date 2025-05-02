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

using namespace std;

// --- Forward Declarations ---
bool getUserInput(string& input_filename, int& pixel_size);
string generateOutputFilename(const string& input_filename, const string& suffix, const string& extension);
unsigned char* loadImage(const string& filename, int& width, int& height, int& channels);
void pixelateImage(const unsigned char* input_data, int width, int height, int channels, int pixel_size, unsigned char* output_data);
void saveImage(const string& filename, int width, int height, int channels, const unsigned char* data);
stbir_pixel_layout get_pixel_layout(int channels); // Keep this accessible
void* safe_malloc(size_t size); // Keep this accessible

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

// --- New Functions ---

/**
 * @brief Gets user input for image path and block size.
 * @param input_filename Reference to store the input image path.
 * @param pixel_size Reference to store the input block size.
 * @return true if input is valid, false otherwise.
 */
bool getUserInput(string& input_filename, int& pixel_size) {
    cout << "Input image path: ";
    cin >> input_filename;
    cout << "Input block size (e.g., 8, 16): ";
    cin >> pixel_size;

    if (pixel_size <= 0) {
        cerr << "Error: Block size must be a positive integer." << endl;
        return false;
    }
    return true;
}

/**
 * @brief Generates the output filename based on the input filename.
 * @param input_filename The full path to the input image.
 * @param suffix The suffix to add before the extension (e.g., "_pixelated_resize").
 * @param extension The desired output extension (e.g., ".png").
 * @return The generated output filename string.
 */
string generateOutputFilename(const string& input_filename, const string& suffix, const string& extension) {
    string input_dir = "";
    string input_base_name_no_ext = "";
    size_t last_slash_pos = input_filename.find_last_of("/\\");

    if (last_slash_pos != string::npos) {
        input_dir = input_filename.substr(0, last_slash_pos + 1);
        size_t last_dot_pos = input_filename.find_last_of('.');
        if (last_dot_pos != string::npos && last_dot_pos > last_slash_pos) {
            input_base_name_no_ext = input_filename.substr(last_slash_pos + 1, last_dot_pos - (last_slash_pos + 1));
        } else {
            input_base_name_no_ext = input_filename.substr(last_slash_pos + 1);
        }
    } else {
        input_dir = ""; // Current directory
        size_t last_dot_pos = input_filename.find_last_of('.');
        if (last_dot_pos != string::npos) {
            input_base_name_no_ext = input_filename.substr(0, last_dot_pos);
        } else {
            input_base_name_no_ext = input_filename;
        }
    }
    return input_dir + input_base_name_no_ext + suffix + extension;
}

/**
 * @brief Loads an image from the specified file.
 * @param filename Path to the image file.
 * @param width Reference to store the image width.
 * @param height Reference to store the image height.
 * @param channels Reference to store the number of color channels.
 * @return Pointer to the loaded image data. Throws std::runtime_error on failure.
 * @note The caller is responsible for freeing the returned pointer using stbi_image_free().
 */
unsigned char* loadImage(const string& filename, int& width, int& height, int& channels) {
    cout << "Loading image '" << filename << "'..." << endl;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, 0);
    if (!data) {
        std::stringstream ss;
        ss << "Error: Cannot load image " << filename << ". STB Error: " << (stbi_failure_reason() ? stbi_failure_reason() : "Unknown error");
        throw std::runtime_error(ss.str());
    }
    cout << "Image loaded successfully: " << width << "x" << height << ", " << channels << " channels" << std::endl;
    return data;
}

/**
 * @brief Pixelates an image by shrinking and then enlarging it using point sampling.
 * @param input_data Pointer to the original image data.
 * @param width Width of the original image.
 * @param height Height of the original image.
 * @param channels Number of color channels.
 * @param pixel_size The target size of the pixel blocks.
 * @param output_data Pointer to the pre-allocated buffer where the pixelated image will be stored.
 * This buffer MUST be large enough (width * height * channels bytes).
 * @throws std::runtime_error If resizing fails.
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
             input_data, width, height, width * channels,           // Input
             small_image_data, small_width, small_height, small_width * channels, // Output
             layout,                // Pixel Layout
             STBIR_TYPE_UINT8,      // Datatype
             STBIR_EDGE_CLAMP,      // Edge Mode
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
             output_data, width, height, width * channels,        // Output (pre-allocated by caller)
             layout,                // Pixel Layout
             STBIR_TYPE_UINT8,      // Datatype
             STBIR_EDGE_CLAMP,      // Edge Mode
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
 * @param filename The path to save the image to.
 * @param width Width of the image.
 * @param height Height of the image.
 * @param channels Number of color channels.
 * @param data Pointer to the image data to save.
 * @throws std::runtime_error If writing the image fails.
 */
void saveImage(const string& filename, int width, int height, int channels, const unsigned char* data) {
    if (!data) {
        throw std::runtime_error("Error: Cannot save null image data.");
    }
    cout << "Saving image to: " << filename << std::endl;
    int write_result = stbi_write_png(filename.c_str(), width, height, channels, data, width * channels);
    if (!write_result) {
        throw std::runtime_error("Error: Cannot write image to " + filename);
    }
    cout << "Pixelated image saved successfully." << std::endl;
}


// --- Main Function (Orchestrator) ---
int main() {
#ifdef _WIN32
    // Optional: Set console to UTF-8 (consider potential side effects)
    // SetConsoleOutputCP(CP_UTF8);
    // SetConsoleCP(CP_UTF8);
#endif

    string input_filename;
    int pixel_size = 0;
    string output_filename;

    // --- 1. Get User Input ---
    if (!getUserInput(input_filename, pixel_size)) {
        return 1; // Exit if input is invalid
    }

    // --- Timing variables ---
    std::chrono::high_resolution_clock::time_point processing_start_time;
    std::chrono::high_resolution_clock::time_point processing_end_time;
    bool timing_captured = false; // Flag to check if timing block completed

    // --- Image data pointers ---
    int width = 0, height = 0, channels = 0;
    unsigned char* input_image_data = nullptr;
    unsigned char* output_image_data = nullptr;
    bool success = false; // Overall operation success flag

    try {
        // --- 2. Generate Output Filename ---
        output_filename = generateOutputFilename(input_filename, "_pixelated_resize", ".png");

        // --- 3. Load Input Image ---
        input_image_data = loadImage(input_filename, width, height, channels);

        // --- 4. Allocate Memory for Output Image ---
        size_t output_image_size = static_cast<size_t>(width) * height * channels;
        output_image_data = static_cast<unsigned char*>(safe_malloc(output_image_size));

        // <<< --- START TIMING for Pixelation Process --- >>>
        processing_start_time = std::chrono::high_resolution_clock::now();

        // --- 5. Perform Pixelation ---
        pixelateImage(input_image_data, width, height, channels, pixel_size, output_image_data);

        // <<< --- END TIMING for Pixelation Process --- >>>
        processing_end_time = std::chrono::high_resolution_clock::now();
        timing_captured = true; // Mark that the timed block completed

        // --- 6. Save Processed Image ---
        saveImage(output_filename, width, height, channels, output_image_data);

        success = true; // Mark overall success

    } catch (const std::exception& e) {
        cerr << "An error occurred: " << e.what() << endl;
        success = false;
        // Timing might not have been captured if error occurred within the timed block
    }

    // --- 7. Free all allocated memory ---
    stbi_image_free(input_image_data); // Free data loaded by stbi_load
    free(output_image_data);           // Free data allocated by safe_malloc

    // --- 8. Output timing results ---
    cout << "-------------------------------------------" << endl;
    if (timing_captured) {
        chrono::duration<double, std::milli> duration_ms = processing_end_time - processing_start_time;
        cout << "Pixelation Processing Time:" << endl;
        cout << std::fixed << std::setprecision(3)
             << duration_ms.count() / 1000.0 << " seconds ("
             << duration_ms.count() << " ms)" << std::endl;
    } else if (success) {
         // Should not happen if success is true, but as a safeguard
         cout << "Pixelation successful, but timing was not captured (unexpected)." << endl;
    }
    else {
        cout << "Pixelation process did not complete successfully. No timing available." << endl;
    }
    cout << "-------------------------------------------" << endl;


    return success ? 0 : 1; // Return 0 on success, 1 on failure
}