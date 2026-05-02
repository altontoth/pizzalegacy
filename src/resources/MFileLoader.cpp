/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "resources/MFileLoader.h"

#include <cstddef>
#include <fstream>
#include "engine/Fatal.h"

#include "engine/Logger.h"
#include "resources/util/FileUtils.h"

namespace {
inline uint32_t read_uint32_le(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}
} // anonymous namespace

std::vector<AudioAsset> MFileLoader::load(const std::string& filename) {
  std::ifstream is = fileutils::open(filename);
  if (!is.is_open()) {
    fatal("Could not open M file: " + filename);
  }

  // Read entire file into buffer
  is.seekg(0, std::ios::end);
  std::streamsize length = is.tellg();
  is.seekg(0, std::ios::beg);

  std::vector<uint8_t> file_data(static_cast<size_t>(length));
  if (!is.read(reinterpret_cast<char*>(file_data.data()), length)) {
    fatal("Failed to read M file data: " + filename);
  }

  // Read entry count (first 4 bytes)
  if (file_data.size() < 4) {
    fatal("M file too small: " + filename);
  }

  uint32_t entry_count = read_uint32_le(file_data.data());

  // Validate file size
  const size_t header_size = 4 + (entry_count * 12);
  if (file_data.size() < header_size) {
    fatal("M file header truncated: " + filename);
  }

  std::vector<AudioAsset> assets;
  assets.reserve(entry_count);

  for (uint32_t i = 0; i < entry_count; ++i) {
    const size_t entry_offset = 4 + (i * 12);
    const uint8_t* entry_data = file_data.data() + entry_offset;

    uint32_t type = read_uint32_le(entry_data);
    uint32_t offset = read_uint32_le(entry_data + 4);
    uint32_t data_length = read_uint32_le(entry_data + 8);

    AudioAssetType asset_type;
    if (type == 1) {
      asset_type = AudioAssetType::CMF;
    }
    else if (type == 2) {
      asset_type = AudioAssetType::VOC;
    }
    else {
      Logger::warn("Unknown audio asset type {} in {}, skipping", type,
                   filename);
      continue;
    }

    if (offset + data_length > file_data.size()) {
      fatal("M file asset data out of bounds: " + filename);
    }

    AudioAsset asset;
    asset.type = asset_type;
    asset.data.resize(data_length);
    std::copy(file_data.begin() + offset,
              file_data.begin() + offset + data_length, asset.data.begin());
    asset.debug_name = compat::format("{}[{}]", filename, i);

    assets.push_back(std::move(asset));
  }
  return assets;
}
