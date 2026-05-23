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

#include "engine/Fatal.h"
#include "engine/Math.h"
#include "resources/AssetMetadata.h"
#include "resources/Vga.h"

#define DOS_VGA_HEADER_SIZE 7

Surface VgaLoader::load(const std::string& filename) {
  std::string actual_path = filename + ".vga";
  Logger::debug("Loading VGA file: {}", actual_path);
  if (filename.find("euromap") != std::string::npos) {
    actual_path = "gfx/" + actual_path;
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
