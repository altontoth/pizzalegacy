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

} // namespace resources
