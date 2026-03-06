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
// Palette indexes (different for DOS vs Amiga versions)
// Dimensions (for headerless Amiga bitplane files)
struct AssetMetadata {
  std::optional<uint8_t> dos_palette;
  std::optional<uint8_t> amiga_palette;

  // Dimensions (primarily for amiga vga files)
  std::optional<Size> size;

  // Get the palette for the currently active format
  [[nodiscard]] uint8_t palette() const;

  [[nodiscard]] bool has_metadata() const {
    return dos_palette.has_value() || amiga_palette.has_value() ||
           size.has_value();
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
