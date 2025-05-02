import os
import sys
from PIL import Image, ImageDraw, ImageFont
import math
import traceback # Import traceback for better error reporting
import time # Import the time module

# --- Configuration ---
ASCII_CHARS = "$@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~<>i!lI;:,\"^`'. "
OUTPUT_WIDTH_CHARS = 256 # Width of the ASCII representation in characters
FONT_NAMES = ['DejaVuSansMono', 'Consolas', 'Courier New', 'Liberation Mono', 'monospace']
FONT_SIZE = 10
RESIZE_OUTPUT = True # Set to True to resize output PNG to original aspect ratio

# Define supported image file extensions (case-insensitive)
SUPPORTED_IMAGE_EXTENSIONS = ('.png', '.jpg', '.jpeg', '.bmp', '.gif', '.tiff', '.webp')

# --- Color Theme Configuration ---
COLOR_THEMES = {
    "dark":         {"background": "black", "foreground": "white"},
    "green_term":   {"background": "black", "foreground": "lime"},
    "light":        {"background": "white", "foreground": "black"},
    "amber_term":   {"background": "#1c1c1c", "foreground": "#FFBF00"},
    "original":     {"background": "black", "foreground": None}, # Needs color map
    "original_light_bg": {"background": "white", "foreground": None}, # Needs color map
    # Add any other themes you want here
}

# --- Selection of Themes to Generate ---
THEMES_TO_GENERATE = [
    "dark",
    "green_term",
    "original",
    "original_light_bg",
    "light",
]
# ------------------------------------

# --- Helper Functions ---

def load_font(font_names, size):
    """Tries to load a font from the provided list."""
    # (Renamed from find_font, logic remains the same)
    for name in font_names:
        try:
            # Try loading with specific encoding first (more robust)
            return ImageFont.truetype(name, size, encoding='unic')
        except IOError:
            # Font file not found or cannot be opened
            continue
        except UnicodeDecodeError:
             # Some systems/fonts might throw this, try without encoding
             try:
                 return ImageFont.truetype(name, size)
             except IOError:
                 continue # Still not found
             except Exception as e_inner:
                  print(f"Warning: Error loading font '{name}' without encoding: {e_inner}. Trying next.")
                  continue
        except Exception as e:
            print(f"Warning: Error loading font '{name}' with specific encoding: {e}. Trying next.")
            continue

    print(f"Warning: Could not find any of the preferred fonts: {font_names}. Trying default PIL font.")
    try:
        # Try loading default with size if possible (newer Pillow versions)
        try:
            return ImageFont.load_default(size=size)
        except TypeError:
            # Fallback for older Pillow or if size isn't supported for default
            print("Note: Using basic ImageFont.load_default() without size argument.")
            return ImageFont.load_default()
    except IOError as e_default_io:
        raise IOError(f"Could not load any suitable font, including the default PIL font. Error: {e_default_io}")
    except Exception as e_default:
        raise IOError(f"An unexpected error occurred loading the default font: {e_default}")


def image_to_ascii(color_image, width_chars, active_theme_name, needs_color_map=False):
    """
    Converts a PIL color image to a list of ASCII strings based on brightness.
    Optionally returns a resized color map image if needs_color_map is True.
    (Internal logic remains the same)
    """
    try:
        image_rgb = color_image.convert('RGB')
        image_gray = image_rgb.convert('L')

        original_width, original_height = image_rgb.size
        aspect_ratio = original_height / float(original_width) if original_width > 0 else 1

        # Adjust for character aspect ratio (characters are often taller than wide)
        char_aspect_ratio_correction = 0.5 # Adjust this value based on font appearance
        new_height_chars = int(width_chars * aspect_ratio * char_aspect_ratio_correction)
        new_height_chars = max(1, new_height_chars) # Ensure at least one line

        # Resize grayscale image for brightness mapping
        resized_gray = image_gray.resize((width_chars, new_height_chars), Image.Resampling.NEAREST)
        gray_pixels = list(resized_gray.getdata())

        color_map_image = None
        if needs_color_map:
            # Resize original color image for pixel-by-pixel color sampling if needed
            color_map_image = image_rgb.resize((width_chars, new_height_chars), Image.Resampling.NEAREST)

        ascii_str_list = []
        num_chars = len(ASCII_CHARS)

        # Determine if background is light or dark to invert brightness mapping if needed
        theme_bg = COLOR_THEMES.get(active_theme_name, {}).get("background", "black").lower()
        is_light_bg = theme_bg in ["white", "#ffffff", "lightgrey", "#d3d3d3", "#fff", "ivory"] # Add more light colors if needed

        pixel_index = 0
        for _ in range(new_height_chars):
            line = ""
            for _ in range(width_chars):
                if pixel_index < len(gray_pixels):
                    pixel_value = gray_pixels[pixel_index]
                    # Map brightness to character index
                    # For light backgrounds, brighter pixels -> denser chars (start of ASCII_CHARS)
                    # For dark backgrounds, brighter pixels -> lighter chars (end of ASCII_CHARS), so invert mapping
                    if is_light_bg:
                        # Higher pixel value (brighter) should map to lower index (denser char)
                        char_index = min(int((255 - pixel_value) / 256 * num_chars), num_chars - 1)
                    else:
                        # Higher pixel value (brighter) should map to higher index (lighter char)
                        char_index = min(int(pixel_value / 256 * num_chars), num_chars - 1)
                    line += ASCII_CHARS[char_index]
                    pixel_index += 1
                else:
                     line += " " # Handle potential rounding errors if pixel_index exceeds length
            # Ensure line has the correct width (safety measure)
            if len(line) != width_chars:
                 line = (line + " " * width_chars)[:width_chars]
            ascii_str_list.append(line)

        return ascii_str_list, color_map_image

    except Exception as e:
        print(f"Error during ASCII conversion for theme '{active_theme_name}': {e}")
        traceback.print_exc()
        return None, None


def create_ascii_png(ascii_lines,
                     color_map_image,
                     theme_name,
                     output_path,
                     font,
                     background_color,
                     foreground_color, # Used for non-original themes
                     original_image_size=None):
    """
    Creates a PNG image from ASCII lines. Uses colors from color_map_image
    if theme_name indicates an original color theme, otherwise uses foreground_color.
    Optionally resizes the output PNG.
    (Internal logic remains the same)
    """
    if not ascii_lines or not ascii_lines[0]:
        print("Error: No ASCII data or empty lines to create PNG.")
        return False # Indicate failure

    is_original_color_theme = theme_name in ["original", "original_light_bg"]

    try:
        # --- Determine text rendering dimensions accurately ---
        dummy_img = Image.new('RGB', (1, 1))
        draw = ImageDraw.Draw(dummy_img)

        # Use textbbox for more accurate sizing if available (Pillow >= 8.0.0)
        # Include descenders/ascenders in line height calculation
        try:
            # Use characters with ascenders and descenders for height
            bbox = draw.textbbox((0, 0), '|M_g(`', font=font)
            line_height = bbox[3] - bbox[1]
            # Get width using the first line of actual text
            bbox_width = draw.textbbox((0, 0), ascii_lines[0], font=font)
            text_width = bbox_width[2] - bbox_width[0]
        except AttributeError:
            # Fallback for older Pillow versions using textsize/textlength
            print("Warning: Using older Pillow text measurement methods (textsize/textlength). Sizing might be less accurate.")
            try:
                 (_, h) = draw.textsize('|M_g(`', font=font)
                 line_height = h
                 (w, _) = draw.textsize(ascii_lines[0], font=font)
                 text_width = w
            except AttributeError: # Even older fallback if textsize fails for complex strings
                 (_, h) = draw.textsize('M', font=font)
                 line_height = int(h * 1.2) # Approximate
                 (w, _) = draw.textsize('M'*len(ascii_lines[0]), font=font)
                 text_width = w # Rough estimate

        line_spacing = line_height + 2 # Add small spacing between lines
        if line_spacing <=0 : line_spacing = font.size + 2
        if text_width <= 0: text_width = font.size * len(ascii_lines[0]) # Estimate if calculation failed

        img_width = max(1, text_width)
        img_height = max(1, line_spacing * len(ascii_lines))

        # --- Create the image and draw text ---
        output_image = Image.new('RGB', (img_width, img_height), color=background_color)
        draw = ImageDraw.Draw(output_image)

        y_text = 0
        if is_original_color_theme and color_map_image:
            # --- Draw character by character using color map ---
            # Estimate average character width for positioning
            # More accurate: measure each char, but slower. Use average for speed.
            avg_char_width = text_width / len(ascii_lines[0]) if ascii_lines[0] else font.size
            for y, line in enumerate(ascii_lines):
                x_pos = 0
                for x, char in enumerate(line):
                    try:
                        # Get color from the resized color map image
                        char_color = color_map_image.getpixel((x, y))
                        # Draw the single character
                        draw.text((math.floor(x_pos), y_text), char, font=font, fill=char_color)
                        # Advance x position - using measured width of *this* char would be best but slow
                        # Using average width is a good compromise
                        x_pos += avg_char_width
                    except IndexError:
                        print(f"Warning: Coordinate ({x},{y}) out of bounds for color_map_image. Using fallback color.")
                        draw.text((math.floor(x_pos), y_text), char, font=font, fill="red") # Fallback color
                        x_pos += avg_char_width
                    except Exception as char_err:
                        print(f"Warning: Error drawing char '{char}' at text pos ({x_pos:.0f},{y_text}): {char_err}")
                        x_pos += avg_char_width # Still advance position
                y_text += line_spacing
        else:
            # --- Draw line by line using fixed foreground color ---
            if foreground_color is None:
                print(f"Error: Foreground color is missing for non-original theme '{theme_name}'. Using white.")
                effective_fg_color = "white"
            else:
                effective_fg_color = foreground_color
            for line in ascii_lines:
                draw.text((0, y_text), line, font=font, fill=effective_fg_color)
                y_text += line_spacing

        # --- Optional Resizing ---
        if RESIZE_OUTPUT and original_image_size:
            original_width, original_height = original_image_size
            if original_width > 0 and original_height > 0:
                # Calculate target height maintaining aspect ratio based on text width
                original_aspect = original_height / float(original_width)
                target_height = int(img_width * original_aspect)
                target_height = max(1, target_height) # Ensure height is at least 1
                try:
                    # Use LANCZOS for high-quality resizing if available
                    output_image = output_image.resize((img_width, target_height), Image.Resampling.LANCZOS)
                except AttributeError:
                    # Fallback for older Pillow versions
                    try:
                        output_image = output_image.resize((img_width, target_height), Image.LANCZOS)
                    except AttributeError:
                        print("Warning: Using older Pillow resize filter (BILINEAR).")
                        output_image = output_image.resize((img_width, target_height), Image.BILINEAR)
            else:
                print("Warning: Cannot resize, original image dimensions are invalid.")
        elif RESIZE_OUTPUT:
             print(f"Warning: Resizing requested but original image size not provided.")

        # --- Save the final image ---
        output_image.save(output_path)
        # final_w, final_h = output_image.size # Get final size after potential resize
        # print(f"      ASCII PNG image saved successfully to: {os.path.basename(output_path)} ({final_w}x{final_h})")
        return True # Indicate success

    except Exception as e:
        print(f"Error creating or saving PNG for theme '{theme_name}' at path '{output_path}': {e}")
        traceback.print_exc()
        return False # Indicate failure

# --- Core Processing Functions ---

def process_image_to_ascii_themes(image_path, font, themes_config, output_dir):
    """
    Processes a single image file: loads it, generates ASCII art for multiple themes,
    and saves them as PNGs in the specified output directory.

    Args:
        image_path (str): Path to the input image file.
        font (ImageFont): The loaded PIL font object.
        themes_config (dict): The COLOR_THEMES dictionary.
        output_dir (str): The directory where output PNGs should be saved.

    Returns:
        dict: A dictionary containing counts {'success': int, 'failed': int}.
    """
    print(f"\nProcessing image: {image_path}")
    results = {'success': 0, 'failed': 0}
    img_load_start = time.perf_counter()
    original_img = None
    original_dimensions = (0, 0)

    # --- Load Image ---
    try:
        with Image.open(image_path) as img_opened:
            original_img = img_opened.convert('RGB') # Ensure RGB
            original_dimensions = original_img.size
        if not original_img:
             raise ValueError("Image could not be loaded or converted.")
        img_load_end = time.perf_counter()
        print(f"  Image loaded ({original_dimensions[0]}x{original_dimensions[1]}) in {img_load_end - img_load_start:.4f}s")
    except FileNotFoundError:
        print(f"  Error: Image file not found at '{image_path}'. Skipping.")
        results['failed'] = len(THEMES_TO_GENERATE) # Count all potential themes as failed for this image
        return results
    except Exception as e:
        print(f"  Error opening or converting image file '{os.path.basename(image_path)}': {e}")
        traceback.print_exc()
        results['failed'] = len(THEMES_TO_GENERATE) # Count all potential themes as failed
        return results

    # --- Ensure Output Directory Exists ---
    try:
        os.makedirs(output_dir, exist_ok=True)
    except OSError as e:
        print(f"  Error: Could not create output directory '{output_dir}': {e}. Skipping image.")
        results['failed'] = len(THEMES_TO_GENERATE)
        return results

    # --- Process Each Theme ---
    base_name = os.path.basename(image_path)
    file_name_no_ext, _ = os.path.splitext(base_name)

    for theme_name in THEMES_TO_GENERATE:
        theme_start_time = time.perf_counter()
        print(f"  - Processing theme: '{theme_name}'...")

        theme_details = themes_config.get(theme_name)
        if not theme_details:
            print(f"    Warning: Theme '{theme_name}' not found in config. Skipping.")
            results['failed'] += 1
            continue

        bg_color = theme_details["background"]
        fg_color = theme_details.get("foreground") # Can be None
        is_original = theme_name in ["original", "original_light_bg"]

        # 1. Convert to ASCII
        ascii_conv_start = time.perf_counter()
        ascii_data, color_map = image_to_ascii(
            color_image=original_img,
            width_chars=OUTPUT_WIDTH_CHARS,
            active_theme_name=theme_name,
            needs_color_map=is_original
        )
        ascii_conv_end = time.perf_counter()

        if not ascii_data:
            print(f"    Error: Failed to generate ASCII data for theme '{theme_name}'. Skipping PNG creation.")
            results['failed'] += 1
            continue
        if is_original and not color_map:
            print(f"    Error: Failed to generate color map needed for original theme '{theme_name}'. Skipping PNG creation.")
            results['failed'] += 1
            continue
        if not is_original and fg_color is None:
             print(f"    Error: Theme '{theme_name}' needs a 'foreground' color. Skipping PNG creation.")
             results['failed'] += 1
             continue

        print(f"      ASCII conversion took: {ascii_conv_end - ascii_conv_start:.4f}s")

        # 2. Create PNG
        resize_suffix = "_resized" if RESIZE_OUTPUT else ""
        output_filename = f"{file_name_no_ext}_ascii_{theme_name}{resize_suffix}.png"
        output_filepath = os.path.join(output_dir, output_filename)

        png_create_start = time.perf_counter()
        png_success = create_ascii_png(
            ascii_lines=ascii_data,
            color_map_image=color_map,
            theme_name=theme_name,
            output_path=output_filepath,
            font=font,
            background_color=bg_color,
            foreground_color=fg_color,
            original_image_size=original_dimensions
        )
        png_create_end = time.perf_counter()

        if png_success:
            results['success'] += 1
            print(f"      PNG creation took: {png_create_end - png_create_start:.4f}s")
            print(f"      Output saved: {output_filename}")
        else:
            results['failed'] += 1
            print(f"      Error: Failed to create PNG for theme '{theme_name}'.")

        theme_end_time = time.perf_counter()
        print(f"    Theme '{theme_name}' processed in {theme_end_time - theme_start_time:.4f}s")

    return results


def process_directory(dir_path, font, themes_config):
    """
    Scans a directory, processes all supported images using
    process_image_to_ascii_themes, and aggregates the results.

    Args:
        dir_path (str): Path to the input directory.
        font (ImageFont): The loaded PIL font object.
        themes_config (dict): The COLOR_THEMES dictionary.

    Returns:
        dict: Aggregated results {'processed_files': int, 'total_success': int,
               'total_failed': int, 'output_location': str or None}.
    """
    print(f"\nProcessing directory: {dir_path}")
    overall_results = {'processed_files': 0, 'total_success': 0, 'total_failed': 0, 'output_location': None}

    # Create a single output directory for all results from this input directory
    dir_name = os.path.basename(os.path.abspath(dir_path))
    parent_dir = os.path.dirname(os.path.abspath(dir_path))
    main_output_dir = os.path.join(parent_dir, f"{dir_name}_ascii_art")
    overall_results['output_location'] = main_output_dir

    try:
        os.makedirs(main_output_dir, exist_ok=True)
        print(f"Output will be saved in: {main_output_dir}")
    except OSError as e:
        print(f"Error: Cannot create main output directory '{main_output_dir}': {e}. Aborting.")
        # Mark all potential files as failed? Difficult to estimate. Mark dir as failed.
        overall_results['total_failed'] = 1 # Indicate directory level failure
        return overall_results

    print("Scanning for supported image files...")
    found_files = []
    try:
        for entry in os.scandir(dir_path):
            if entry.is_file() and entry.name.lower().endswith(SUPPORTED_IMAGE_EXTENSIONS):
                found_files.append(entry.path)
    except FileNotFoundError:
        print(f"Error: Input directory '{dir_path}' not found during scan.")
        overall_results['total_failed'] = 1
        return overall_results
    except Exception as e:
        print(f"Error scanning directory '{dir_path}': {e}")
        overall_results['total_failed'] = 1
        return overall_results

    if not found_files:
        print("No supported image files found in the directory.")
        return overall_results

    print(f"Found {len(found_files)} potential image file(s). Starting processing...")
    overall_results['processed_files'] = len(found_files)

    for image_file_path in found_files:
        # Process each image, saving results into the single main_output_dir
        image_results = process_image_to_ascii_themes(
            image_path=image_file_path,
            font=font,
            themes_config=themes_config,
            output_dir=main_output_dir # All outputs go here
        )
        overall_results['total_success'] += image_results['success']
        overall_results['total_failed'] += image_results['failed']

    return overall_results

# --- Input/Output Functions ---

def get_input_path():
    """Gets the input path (file or directory) from the user."""
    input_path = ""
    while not input_path:
        try:
            input_path = input("Enter the path to the image file or directory: ").strip().strip("'\"")
            if not input_path:
                print("Input cannot be empty.")
            elif not os.path.exists(input_path):
                 print(f"Error: Path does not exist: '{input_path}'")
                 input_path = "" # Ask again
        except KeyboardInterrupt:
            print("\nOperation cancelled by user.")
            return None
    return input_path

def print_summary(results, duration, font_load_duration):
    """Prints the final processing summary."""
    print("\n===================================")
    print("        Processing Summary")
    print("===================================")

    input_type = results.get('input_type', 'unknown')
    output_location = results.get('output_location')

    if input_type == 'invalid':
        print("Status: Failed (Invalid input path)")
    elif input_type == 'font_error':
        print("Status: Failed (Could not load required font)")
    elif input_type == 'file':
        success_count = results.get('total_success', 0)
        fail_count = results.get('total_failed', 0)
        print(f"Input Type: Single File")
        print(f"Themes Processed: {success_count + fail_count}")
        print(f"  - Successful PNGs: {success_count}")
        print(f"  - Failed/Skipped Themes: {fail_count}")
        if success_count > 0 and output_location:
            print(f"Output Location: {output_location}")
    elif input_type == 'directory':
        processed_files = results.get('processed_files', 0)
        success_count = results.get('total_success', 0)
        fail_count = results.get('total_failed', 0)
        print(f"Input Type: Directory")
        print(f"Image Files Found/Attempted: {processed_files}")
        print(f"Total PNGs Generated Successfully: {success_count}")
        print(f"Total Failed/Skipped Theme Attempts: {fail_count}")
        if output_location:
             print(f"Output Location: {output_location}")

    print("-----------------------------------")
    print("Execution Time:")
    print(f"  - Font Loading: {font_load_duration:.4f} seconds")
    print(f"  - Total Processing: {duration:.4f} seconds") # Includes all image processing
    print("===================================")


# --- Main Orchestration ---
def main():
    """Main execution function."""
    print("--- ASCII Art Generator ---")

    # --- Load Font (Essential, do first) ---
    print("Loading font...")
    font_load_start = time.perf_counter()
    try:
        font = load_font(FONT_NAMES, FONT_SIZE)
    except Exception as e:
        font_load_end = time.perf_counter()
        print(f"Fatal Error: Could not load font. {e}")
        results = {'input_type': 'font_error'}
        print_summary(results, 0, font_load_end - font_load_start)
        sys.exit(1)
    font_load_end = time.perf_counter()
    font_load_duration = font_load_end - font_load_start
    print(f"Font loaded in {font_load_duration:.4f} seconds.")

    # --- Get Input Path ---
    input_path = get_input_path()
    if input_path is None:
        sys.exit(0) # User cancelled

    # --- Start Processing Timer ---
    processing_start_time = time.perf_counter()
    results = {} # Initialize results dict

    # --- Determine Path Type and Process ---
    if os.path.isfile(input_path):
        results['input_type'] = 'file'
        # Define output dir relative to the input file's location
        file_dir = os.path.dirname(os.path.abspath(input_path))
        file_name_no_ext, _ = os.path.splitext(os.path.basename(input_path))
        output_dir = os.path.join(file_dir, f"{file_name_no_ext}_ascii_art")
        results['output_location'] = output_dir
        # Process the single image
        img_results = process_image_to_ascii_themes(input_path, font, COLOR_THEMES, output_dir)
        results['total_success'] = img_results['success']
        results['total_failed'] = img_results['failed']

    elif os.path.isdir(input_path):
        results['input_type'] = 'directory'
        # Process the directory
        dir_results = process_directory(input_path, font, COLOR_THEMES)
        results.update(dir_results) # Merge results from process_directory

    else:
        # Should have been caught by get_input_path, but as a fallback
        print(f"Error: Input path '{input_path}' is not a valid file or directory.")
        results['input_type'] = 'invalid'

    # --- Stop Processing Timer ---
    processing_end_time = time.perf_counter()
    total_processing_duration = processing_end_time - processing_start_time

    # --- Print Summary ---
    print_summary(results, total_processing_duration, font_load_duration)


if __name__ == "__main__":
    main()