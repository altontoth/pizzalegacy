/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "resources/PaletteLoader.h"

#include <cstdint>
#include <fstream>
#include <ios>
#include <vector>

#include "engine/Fatal.h"
#include "engine/gfx/PaletteBank.h"
#include "resources/util/FileUtils.h"

namespace resources {

namespace {

struct CursorColor {
  uint8_t index;
  gfx::Color color;
};

// The three colors the mouse cursor is drawn in, taken from palette 38.
constexpr CursorColor cursor_colors[] = {
    {fallback_palette::cursor_red, gfx::Color{240, 96, 80}},
    {18, gfx::Color{240, 144, 128}},
    {19, gfx::Color{240, 160, 160}},
};

} // namespace

void load_palette_bank() {
  // Load DOS PALETTE.E: 71 palettes * 256 colors * 3 bytes (6-bit RGB)
  constexpr std::size_t DOS_FILE_SIZE = gfx::VGA_PALETTE_COUNT *
                                        gfx::VGA_COLORS_PER_PALETTE *
                                        gfx::VGA_BYTES_PER_COLOR;

  std::ifstream is = fileutils::open("gfx/palette.e", std::ios::binary);
  if (!is) {
    fatal("Failed to open gfx/palette.e");
  }

  std::vector<uint8_t> dos_raw(DOS_FILE_SIZE);
  if (!is.read(reinterpret_cast<char*>(dos_raw.data()), DOS_FILE_SIZE)) {
    fatal("Failed to read full palette.e");
  }

  gfx::PaletteBank::instance().load_dos_palette(dos_raw);
}

void load_fallback_palette() {
  std::vector<gfx::Color> palette(gfx::COLORS_PER_PALETTE, gfx::Color{0, 0, 0});
  palette[fallback_palette::white] = gfx::Color{252, 252, 252};
  for (const CursorColor& color : cursor_colors) {
    palette[color.index] = color.color;
  }
  gfx::PaletteBank::instance().load_placeholder(palette);
}

} // namespace resources
