/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "resources/BobLoader.h"

#include <cstddef>
#include <fstream>
#include <vector>
#include "engine/Endian.h"
#include "engine/Fatal.h"
#include "resources/util/FileUtils.h"

std::unique_ptr<Bob> BobLoader::load(const std::string& filename) {
  std::ifstream is = fileutils::open(filename);

  is.seekg(0, std::ios::end);
  std::streamsize length = is.tellg();
  is.seekg(0, std::ios::beg);

  std::vector<uint8_t> raw_data(static_cast<std::size_t>(length));

  // Read the entire file directly into it
  if (!is.read(reinterpret_cast<char*>(raw_data.data()), length)) {
    fatal("Failed to read BOB file data");
  }

  // Parse the header (state_count, etc.)
  auto state_count =
      static_cast<std::size_t>(endian::u16le(raw_data[0], raw_data[1]));

  const std::size_t file_header_size = 6;
  const std::size_t frame_header_size = 7;
  std::vector<FrameInfo> frames(state_count);

  std::size_t sum_of_previous_pixels = 0;
  for (std::size_t s = 0; s < state_count; ++s) {
    std::size_t idx = file_header_size + (s * frame_header_size);
    uint8_t w = raw_data[idx + 0];
    uint8_t h = raw_data[idx + 1];
    uint8_t padding1 = raw_data[idx + 2];
    uint8_t u2 = raw_data[idx + 3];
    uint8_t u3 = raw_data[idx + 4];
    uint8_t u4 = raw_data[idx + 5];
    uint8_t padding2 = raw_data[idx + 6];

    // offset for pixel data:
    // pixel data starts after the last frame's data
    // plus 6 + state_count*7 for all headers
    frames[s].width = w;
    frames[s].height = h;
    frames[s].data_offset = file_header_size +
                            (state_count * frame_header_size) +
                            sum_of_previous_pixels;

    // next frame offset:
    sum_of_previous_pixels += (static_cast<std::size_t>(w * h));
  }

  return std::make_unique<Bob>(filename, std::move(raw_data),
                               std::move(frames));
}
