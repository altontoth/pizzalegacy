/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "resources/BobLoader.h"

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "resources/AmigaBobLoader.h"
#include "resources/util/Platform.h"

namespace {
// Translate DOS BOB paths to Amiga paths
std::string translate_amiga_path(const std::string& dos_path) {
  if (!platform::is_amiga()) {
    return dos_path;
  }

  // Mapping of DOS filename exceptions to Amiga names with special characters
  static const std::unordered_map<std::string, std::string> name_map = {
      {"moebel", "möbel"},
      {"kuebobs", "kübobs"},
      {"gaeste", "gäste"},
  };

  std::string path = dos_path;

  // Always translate bob/ to gfx/ on Amiga
  if (path.starts_with("bob/")) {
    path = "gfx/" + path.substr(4);
  }

  // Check if we need to translate the filename itself
  // Extract just the filename without extension
  size_t last_slash = path.rfind('/');
  size_t dot_pos = path.rfind('.');
  std::string dir =
      (last_slash != std::string::npos) ? path.substr(0, last_slash + 1) : "";
  std::string basename =
      (last_slash != std::string::npos) ? path.substr(last_slash + 1) : path;

  // Remove extension if present
  if (dot_pos != std::string::npos && dot_pos > last_slash) {
    basename = basename.substr(
        0, dot_pos - (last_slash != std::string::npos ? last_slash + 1 : 0));
  }

  // Look up the translation
  auto it = name_map.find(basename);
  if (it != name_map.end()) {
    path = dir + it->second;
    Logger::debug("Translated BOB path: {} -> {}", dos_path, path);
  }

  return path;
}
} // anonymous namespace

std::unique_ptr<Bob> BobLoader::load(const std::string& filename) {
  std::string actual_path = translate_amiga_path(filename);
  if (platform::is_amiga()) {
    return AmigaBobLoader::load(actual_path);
  }

  std::ifstream is = fileutils::open(actual_path);
  if (!is.is_open()) {
    fatal("Could not open BOB file: " + actual_path);
  }

  is.seekg(0, std::ios::end);
  std::streamsize length = is.tellg();
  is.seekg(0, std::ios::beg);

  std::vector<uint8_t> raw_data(static_cast<std::size_t>(length));

  // Read the entire file directly into it
  if (!is.read(reinterpret_cast<char*>(raw_data.data()), length)) {
    fatal("Failed to read BOB file data");
  }

  // Parse the header (state_count, etc.)
  auto state_count = static_cast<std::size_t>(
      math::two_bytes_to_uint16(raw_data[0], raw_data[1]));

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

  return std::make_unique<Bob>(actual_path, std::move(raw_data),
                               std::move(frames));
}
