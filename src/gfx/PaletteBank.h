/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

// PaletteBank - loads and caches the 71 VGA palettes (palette.e) lazily.

#include "engine/Fatal.h"
#include "gfx/Color.h"
#include "gfx/PaletteHandle.h"
#include "resources/util/FileUtils.h"

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

// Palette storage: 71 palettes x 256 colors x 3 bytes
constexpr int PALETTE_COUNT = 71;
constexpr int COLORS_PER_PALETTE = 256;
constexpr int BYTES_PER_COLOR = 3;

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

  // raw, no override. Returns const reference to 256 Color entries
  const std::vector<Color>& colors(PaletteHandle h);

  // copy with override applied
  std::vector<Color> colors(PaletteHandle base, const PaletteOverride& ovr);

private:
  PaletteBank() { load_palette_file(); }

  void load_palette_file();
  void load_dos_palette();

  /* Packed raw data: 71 palettes x 256 colors x 3 bytes */
  std::vector<uint8_t> raw_;
  std::array<std::vector<Color>, PALETTE_COUNT> cache_;
  std::bitset<PALETTE_COUNT> loaded_;
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

inline void PaletteBank::load_palette_file() {
  if (!raw_.empty()) {
    return;
  }

  constexpr std::size_t TOTAL_SIZE =
      PALETTE_COUNT * COLORS_PER_PALETTE * BYTES_PER_COLOR;
  raw_.resize(TOTAL_SIZE, 0);

  load_dos_palette();
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

} // namespace gfx
