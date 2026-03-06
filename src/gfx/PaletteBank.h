/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

// PaletteBank - loads and caches the 71 VGA palettes (palette.e) lazily.

#include "core/util/ConfigManager.h"
#include "engine/Fatal.h"
#include "gfx/Color.h"
#include "resources/util/FileUtils.h"
#include "resources/util/Platform.h"

#include <array>
#include <bitset>
#include <cstdint>
#include <fstream>
#include <vector>

namespace gfx {

// DOS VGA format (6-bit RGB)
constexpr int VGA_PALETTE_COUNT = 71;
constexpr int VGA_COLORS_PER_PALETTE = 256;
constexpr int VGA_BYTES_PER_COLOR = 3;

// Amiga format (5-bit RGB in 16-bit, 32 colors per palette)
constexpr int AMIGA_PALETTE_COUNT = 67;
constexpr int AMIGA_COLORS_PER_PALETTE = 32;
constexpr int AMIGA_BYTES_PER_COLOR = 2;

// Normalized storage (both formats stored as 71 palettes x 256 colors x 3
// bytes)
constexpr int PALETTE_COUNT = 71;
constexpr int COLORS_PER_PALETTE = 256;
constexpr int BYTES_PER_COLOR = 3;

struct PaletteHandle {
  uint8_t id{0};
}; // 0-70 inclusive

// PaletteOverride - Describes a palette modification relative to a base
// palette. We specify a list of destination indices to overwrite in the base
// palette, and a parallel list of source indices (from another palette) to copy
// into those slots. Both arrays must be the same length.
//
// Usage:
//   PaletteOverride ovr;
//   ovr.dst_indices    = {100, 101, 102};   // indices in base palette
//   ovr.src_indices    = {100, 101, 102};   // indices in source palette
//   ovr.src_palette_id = 13;                // pull colors from palette #13
//
//   // Get palette #18 with selected slots replaced by palette #13 colors.
//   auto new_palette = PaletteBank::instance().colors(PaletteHandle{18}, ovr);
//
//   // Apply the overridden palette to a surface:
//   surface.colors(new_palette);
//
// - To revert a Surface to its base palette, call
// `surface.clear_custom_palette()`.
struct PaletteOverride {
  std::vector<uint8_t> dst_indices;
  std::vector<uint8_t> src_indices;
  uint8_t src_palette_id{0};
};

class PaletteBank {
public:
  static PaletteBank& instance() {
    static PaletteBank self;
    return self;
  }

  // Set ConfigManager reference for graphics format detection (DOS vs Amiga)
  // Should be called during Game::init() before any palettes are accessed
  static void config_manager(ConfigManager* config);

  // raw, no override. Returns const reference to 256 Color entries
  const std::vector<Color>& colors(PaletteHandle h);

  // copy with override applied
  std::vector<Color> colors(PaletteHandle base, const PaletteOverride& ovr);

private:
  PaletteBank() { load_palette_file(); }

  void load_palette_file();
  void load_dos_palette();
  void load_amiga_palette();

  /* Packed raw data - 71x256x3 bytes (normalized for both DOS and Amiga) */
  std::vector<uint8_t> raw_;
  std::array<std::vector<Color>, PALETTE_COUNT> cache_;
  std::bitset<PALETTE_COUNT> loaded_;

  // ConfigManager reference for graphics format detection
  static ConfigManager* s_config_manager;
};

inline const std::vector<Color>& PaletteBank::colors(PaletteHandle h) {
  if (h.id >= PALETTE_COUNT) {
    throw std::out_of_range("Palette id out of range" + std::to_string(h.id));
  }

  if (!loaded_.test(h.id)) {
    const std::size_t PALETTE_SIZE = COLORS_PER_PALETTE * BYTES_PER_COLOR;
    std::size_t offset = h.id * PALETTE_SIZE;

    std::vector<Color>& dst = cache_[h.id];
    dst.resize(COLORS_PER_PALETTE);

    for (int i = 0; i < COLORS_PER_PALETTE; ++i) {
      std::size_t idx = offset + (i * BYTES_PER_COLOR);
      dst[i].r = raw_[idx + 0];
      dst[i].g = raw_[idx + 1];
      dst[i].b = raw_[idx + 2];
    }
    loaded_.set(h.id);
  }
  return cache_[h.id];
}

inline std::vector<Color> PaletteBank::colors(PaletteHandle base,
                                              const PaletteOverride& ovr) {

  auto out = colors(base); // copy

  if (ovr.dst_indices.size() != ovr.src_indices.size()) {
    fatal("override index arrays must match length");
  }

  const auto& src_pal = colors(PaletteHandle{ovr.src_palette_id});

  for (std::size_t i = 0; i < ovr.dst_indices.size(); ++i) {
    out[ovr.dst_indices[i]] = src_pal[ovr.src_indices[i]];
  }
  return out;
}

inline ConfigManager* PaletteBank::s_config_manager = nullptr;

inline void PaletteBank::config_manager(ConfigManager* config) {
  s_config_manager = config;
}

inline void PaletteBank::load_palette_file() {
  if (!raw_.empty()) {
    return;
  }

  constexpr std::size_t TOTAL_SIZE =
      PALETTE_COUNT * COLORS_PER_PALETTE * BYTES_PER_COLOR;
  raw_.resize(TOTAL_SIZE, 0);

  if (platform::is_amiga()) {
    load_amiga_palette();
  }
  else {
    load_dos_palette();
  }
}

inline void PaletteBank::load_dos_palette() {
  // Load DOS PALETTE.E: 71 palettes * 256 colors * 3 bytes (6-bit RGB)
  constexpr std::size_t DOS_FILE_SIZE =
      VGA_PALETTE_COUNT * VGA_COLORS_PER_PALETTE * VGA_BYTES_PER_COLOR;

  std::ifstream is = fileutils::open("gfx/palette.e", std::ios::binary);
  if (!is) {
    fatal("Failed to open gfx/palette.e");
  }

  std::vector<uint8_t> dos_raw(DOS_FILE_SIZE);
  if (!is.read(reinterpret_cast<char*>(dos_raw.data()), DOS_FILE_SIZE)) {
    fatal("Failed to read full palette.e");
  }

  // Convert 6-bit RGB to 8-bit and store in raw_
  for (std::size_t i = 0; i < dos_raw.size(); ++i) {
    raw_[i] = dos_raw[i] << 2; // 6-bit to 8-bit: shift left by 2
  }
}

inline void PaletteBank::load_amiga_palette() {
  // Load Amiga palette: 67 palettes * 32 colors * 2 bytes (5-5-5 RGB in 16-bit)
  // Pad to 71 palettes (4 extra are filled with black already)

  constexpr std::size_t AMIGA_FILE_SIZE =
      AMIGA_PALETTE_COUNT * AMIGA_COLORS_PER_PALETTE * AMIGA_BYTES_PER_COLOR;

  std::ifstream is = fileutils::open("gfx/palette", std::ios::binary);
  if (!is) {
    fatal("Failed to open gfx/palette (Amiga palette file). "
          "Make sure the file exists in the gfx/ directory.");
  }

  std::vector<uint8_t> amiga_raw(AMIGA_FILE_SIZE);
  if (!is.read(reinterpret_cast<char*>(amiga_raw.data()), AMIGA_FILE_SIZE)) {
    fatal("Failed to read full Amiga palette file");
  }

  // Convert and expand Amiga palettes to 71 * 256 * 3 format
  for (int pal = 0; pal < AMIGA_PALETTE_COUNT; ++pal) {
    for (int col = 0; col < AMIGA_COLORS_PER_PALETTE; ++col) {
      // Read little-endian 16-bit color from Amiga file
      std::size_t src_idx =
          (pal * AMIGA_COLORS_PER_PALETTE + col) * AMIGA_BYTES_PER_COLOR;
      uint16_t rgb = (amiga_raw[src_idx] << 8) | amiga_raw[src_idx + 1];
      uint8_t r4 = (rgb >> 8) & 0x0F;
      uint8_t g4 = (rgb >> 4) & 0x0F;
      uint8_t b4 = rgb & 0x0F;

      uint8_t r8 = (r4 << 4) | r4;
      uint8_t g8 = (g4 << 4) | g4;
      uint8_t b8 = (b4 << 4) | b4;

      std::size_t dst_idx = (pal * COLORS_PER_PALETTE + col) * BYTES_PER_COLOR;
      raw_[dst_idx] = r8;
      raw_[dst_idx + 1] = g8;
      raw_[dst_idx + 2] = b8;
    }
  }
}

} // namespace gfx
