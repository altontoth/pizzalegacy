/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "resources/VgaLoader.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "core/util/ConfigManager.h"
#include "engine/Fatal.h"
#include "resources/AssetMetadata.h"
#include "resources/BitplaneDecoder.h"
#include "resources/Vga.h"
#include "resources/util/Platform.h"

#define DOS_VGA_HEADER_SIZE 7

ConfigManager* VgaLoader::s_config_manager = nullptr;

void VgaLoader::config_manager(ConfigManager* config) {
  s_config_manager = config;
}

Surface VgaLoader::load(const std::string& filename) {
  bool is_amiga_format = platform::is_amiga();

  // Special case: cursor files are always DOS format
  if (is_amiga_format && filename.find("cursor") != std::string::npos) {
    is_amiga_format = false;
  }

  // Build the actual file path with appropriate extension
  // filename is expected to be like "gfx/back1" (no extension)
  std::string actual_path;

  if (is_amiga_format) {
    // Amiga: try base name first, then try with .f extension
    actual_path = filename;

    if (!fileutils::fexists(actual_path)) {
      // Try with .f extension (menu.f, etc.)
      actual_path = filename + ".f";
      if (!fileutils::fexists(actual_path)) {
        actual_path = filename + ".i";
      }
      if (!fileutils::fexists(actual_path)) {
        actual_path = filename + ".96";
      }
      if (!fileutils::fexists(actual_path)) {
        fatal("Cannot find Amiga VGA file: " + filename +
              " (tried both with and without .f/.i extension)");
      }
    }
    Logger::debug("Loading Amiga VGA file: {}", actual_path);
  }
  else {
    // DOS: always add .vga extension
    actual_path = filename + ".vga";
    Logger::debug("Loading DOS VGA file: {}", actual_path);
    if (filename.find("euromap") != std::string::npos) {
      actual_path = "gfx/" + actual_path;
    }
  }

  auto is = fileutils::open(actual_path, std::ios::binary | std::ios::ate);

  std::streampos length_pos = is.tellg();
  if (length_pos == -1) {
    std::cout << "Failed to get file size via tellg().\n";
  }
  is.seekg(0, std::ifstream::beg);

  auto length =
      static_cast<std::size_t>(static_cast<std::streamoff>(length_pos));

  // Allocate a buffer of this length and read the file in:
  std::vector<char> raw_buffer(length);
  is.read(raw_buffer.data(), static_cast<std::streamsize>(length));
  is.close();

  // If using Amiga bitplane format, decode directly
  if (is_amiga_format) {
    // Read entire file as bitplane data
    std::vector<uint8_t> raw_data(raw_buffer.begin(), raw_buffer.end());

    // Look up dimensions from asset metadata
    std::optional<Size> size_hint =
        resources::AssetMetadataRegistry::get_size(actual_path);
    if (!size_hint.has_value()) {
      fatal("No size hint found for VGA file: " + actual_path);
    }

    int width = size_hint->w;
    int height = size_hint->h;

    // Calculate bytes per line (rounded up to byte boundary)
    int data_bytes_per_line = (width + 7) / 8;

    // Amiga bitplane files may have 16-byte aligned rows
    int padded_bytes_per_line = ((data_bytes_per_line + 15) / 16) * 16;

    // Try to determine number of bitplanes and padding from file size
    // Most common: 5 bitplanes (32 colors)
    size_t expected_5plane_unpadded = data_bytes_per_line * height * 5;
    size_t expected_5plane_padded = padded_bytes_per_line * height * 5;

    // Also try 4 bitplanes (16 colors) for smaller files
    size_t expected_4plane_unpadded = data_bytes_per_line * height * 4;
    size_t expected_4plane_padded = padded_bytes_per_line * height * 4;

    // Also try 3 bitplanes (8 colors) for even smaller files
    size_t expected_3plane_unpadded = data_bytes_per_line * height * 3;
    size_t expected_3plane_padded = padded_bytes_per_line * height * 3;

    int bytes_per_line = -1;
    int num_bitplanes = 5; // default

    if (length == expected_5plane_unpadded) {
      bytes_per_line = data_bytes_per_line;
      num_bitplanes = 5;
      Logger::info("Detected: 5 bitplanes, unpadded ({} bytes/line)",
                   bytes_per_line);
    }
    else if (length == expected_5plane_padded) {
      bytes_per_line = padded_bytes_per_line;
      num_bitplanes = 5;
      Logger::info("Detected: 5 bitplanes, padded ({} bytes/line)",
                   bytes_per_line);
    }
    else if (length == expected_4plane_unpadded) {
      bytes_per_line = data_bytes_per_line;
      num_bitplanes = 4;
      Logger::info("Detected: 4 bitplanes, unpadded ({} bytes/line)",
                   bytes_per_line);
    }
    else if (length == expected_4plane_padded) {
      bytes_per_line = padded_bytes_per_line;
      num_bitplanes = 4;
      Logger::info("Detected: 4 bitplanes, padded ({} bytes/line)",
                   bytes_per_line);
    }
    else if (length == expected_3plane_unpadded) {
      bytes_per_line = data_bytes_per_line;
      num_bitplanes = 3;
      Logger::info("Detected: 3 bitplanes, unpadded ({} bytes/line)",
                   bytes_per_line);
    }
    else if (length == expected_3plane_padded) {
      bytes_per_line = padded_bytes_per_line;
      num_bitplanes = 3;
      Logger::info("Detected: 3 bitplanes, padded ({} bytes/line)",
                   bytes_per_line);
    }
    else {
      // File size doesn't match expected - log details for debugging
      fatal(compat::format("Cannot determine bitplane format for {}: "
                           "file size {} doesn't match expected sizes "
                           "(5pl unpadded={}, 5pl padded={}, "
                           "4pl unpadded={}, 4pl padded={}, "
                           "3pl unpadded={}, 3pl padded={})",
                           filename, length, expected_5plane_unpadded,
                           expected_5plane_padded, expected_4plane_unpadded,
                           expected_4plane_padded, expected_3plane_unpadded,
                           expected_3plane_padded));
    }

    // Get palette from asset metadata
    auto palette_opt = resources::AssetMetadataRegistry::get_palette(filename);
    uint8_t palette_index = palette_opt.value_or(0);

    Surface bitplane_surface =
        BitplaneDecoder::decode(raw_data, width, height, num_bitplanes,
                                bytes_per_line, palette_index, 2);
    return bitplane_surface;
  }

  // DOS RLE format
  // Pointers start at zero, so last element in the array is length - 1:
  --length;

  // Now that we know the size of the file, we can find the rle_threshold which
  // is stored in the last byte of the data.
  int rle_threshold = static_cast<uint8_t>(raw_buffer[length]) - 1;

  // Determine width and height
  auto width = math::two_bytes_to_uint16(raw_buffer[0], raw_buffer[1]);
  auto height = math::two_bytes_to_uint16(raw_buffer[2], raw_buffer[3]);

  // Make sure this looks like a valid VGA file (some files are not displayed as
  // is but are rather collections of smaller images that can together be bigger
  // than 320x200, so that's why height can be up to 320):
  assert(width > 0 && height > 0 && width <= 320 && height <= 320);

  // Allocate memory for the picture in memory:
  std::vector<uint8_t> picture(static_cast<size_t>(width * height));

  // This is the start point of the data in the file:
  std::size_t bytes_processed_count = DOS_VGA_HEADER_SIZE + 1;
  std::size_t picture_index = 0;
  std::size_t expected_pixels = static_cast<std::size_t>(width * height);

  while (bytes_processed_count < length) {
    // Read the control byte
    uint8_t init_data =
        static_cast<uint8_t>(raw_buffer[bytes_processed_count++]);
    if (init_data > rle_threshold) {
      // Repeated pixel branch: the value indicates a run.
      if (bytes_processed_count >= length) {
        std::cerr << "DEBUG: Out-of-bounds: expected pixel for repetition "
                     "after control byte > rle_threshold.\n";
        break;
      }
      auto actual_data =
          static_cast<uint8_t>(raw_buffer[bytes_processed_count++]);
      auto repeat_count = static_cast<std::size_t>(init_data - rle_threshold);
      if ((picture_index + repeat_count) > expected_pixels) {
        std::cerr << "DEBUG: ERROR: Repeated branch would overflow picture "
                     "array. picture_index ("
                  << picture_index << ") + repeat_count (" << repeat_count
                  << ") > expected_pixels (" << expected_pixels << ").\n";
        break;
      }
      std::fill_n(&picture[picture_index], repeat_count, actual_data);
      picture_index += repeat_count;
    }
    else {
      // Literal run branch: the control byte (init_data) indicates that
      // (init_data + 1) literal pixels follow.
      std::size_t literal_count = init_data + 1;
      if (bytes_processed_count + literal_count > length) {
        std::cerr << "DEBUG: Out-of-bounds: literal run expects "
                  << literal_count << " bytes, but only "
                  << (length - bytes_processed_count) << " remain.\n";
        break;
      }
      if (picture_index + literal_count > expected_pixels) {
        std::cerr << "DEBUG: ERROR: Literal run would overflow picture array. "
                     "picture_index ("
                  << picture_index << ") + literal_count (" << literal_count
                  << ") > expected_pixels (" << expected_pixels << ").\n";
        break;
      }
      for (std::size_t i = 0; i < literal_count; i++) {
        auto actual_data =
            static_cast<uint8_t>(raw_buffer[bytes_processed_count + i]);
        std::fill_n(&picture[picture_index + i], 1, actual_data);
      }
      bytes_processed_count += literal_count;
      picture_index += literal_count;
    }
  }

  // Check if decoding completed successfully
  if (picture_index != expected_pixels) {
    std::cerr << "ERROR: VGA RLE decoding incomplete. Decoded " << picture_index
              << " pixels but expected " << expected_pixels << " ("
              << (expected_pixels - picture_index) << " pixels missing).\n";
    std::cerr << "File: " << filename << "\n";
    fatal("VGA RLE decoding failed for: " + filename);
  }

  auto palette_opt = resources::AssetMetadataRegistry::get_palette(filename);
  uint8_t index = palette_opt.value_or(0);

  return Surface::from_indexed(std::move(picture), width, height,
                               gfx::PaletteHandle{index}, 2);
}
