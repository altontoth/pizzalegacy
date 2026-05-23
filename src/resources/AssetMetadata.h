/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include "engine/Size.h"

namespace resources {

// Metadata for legacy asset files (VGA, BOB) from original game.
// Stores per-file information that isn't embedded in the file format:
// Palette index and dimensions.
struct AssetMetadata {
  std::optional<uint8_t> dos_palette;
  std::optional<Size> size;

  // Three-arg constructor for backwards compatibility with {p, p2, size}
  // initializers.
  AssetMetadata(std::optional<uint8_t> p, std::optional<uint8_t> /*unused*/,
                std::optional<Size> s = std::nullopt)
      : dos_palette(p), size(s) {}

  AssetMetadata(std::optional<uint8_t> p, std::optional<Size> s = std::nullopt)
      : dos_palette(p), size(s) {}

  [[nodiscard]] uint8_t palette() const;

  [[nodiscard]] bool has_metadata() const {
    return dos_palette.has_value() || size.has_value();
  }
};

// Central registry of asset metadata for files from the original game.
class AssetMetadataRegistry {
public:
  static std::optional<AssetMetadata> get(const std::string& filename);
  static std::optional<uint8_t> get_palette(const std::string& filename);
  static std::optional<Size> get_size(const std::string& filename);

private:
  static const std::unordered_map<std::string, AssetMetadata> metadata_;
};

} // namespace resources
