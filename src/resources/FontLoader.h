/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <span>
#include <unordered_map>
#include <utfcpp/utf8.h>
#include <vector>
#include "gfx/Surface.h"
#include "resources/Font.h"
#include "resources/FontHeader.h"
#include "resources/FontTypes.h"

class FontLoader {
public:
  [[nodiscard]] static std::unique_ptr<Font> load(const std::string& filename);

private:
  [[nodiscard]] static std::vector<uint8_t>
  load_file_bytes(std::string_view fname);

  struct ParsedFont {
    FontHeader header;
    std::vector<uint8_t> bitmap;              // row-major, as stored
    std::vector<CharacterPosition> positions; // one per glyph
  };

  [[nodiscard]] static ParsedFont parse(std::span<const uint8_t> bytes);

  [[nodiscard]] static int render_scale(FontFace face) noexcept {
    return face == FontFace::MINI ? 1 : 2;
  }

  [[nodiscard]] static gfx::Surface build_atlas(const ParsedFont& pf, int scale,
                                                gfx::PaletteHandle palette);

  [[nodiscard]] static std::unordered_map<uint32_t, Glyph>
  build_glyph_table(const ParsedFont& pf, int scale);
};
