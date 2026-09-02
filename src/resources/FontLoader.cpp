/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "resources/FontLoader.h"

#include <cstddef>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include "engine/Fatal.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"

FontLoader::ParsedFont FontLoader::parse(std::span<const uint8_t> bytes) {
  // Ensure we have at least enough bytes for the font header.
  if (bytes.size() < sizeof(FontHeader)) {
    fatal("font file truncated");
  }

  ParsedFont out{};

  // DOS format: direct binary structure
  std::memcpy(&out.header, bytes.data(), sizeof(FontHeader));

  // Advance the pointer past the header.
  const auto* p = bytes.data() + sizeof(FontHeader);

  // Compute how many bytes are used for the bitmap based on header info.
  const auto bitmap_bytes =
      out.header.number_of_bytes_per_row * out.header.character_height;

  if (bytes.size() < sizeof(FontHeader) + bitmap_bytes) {
    fatal("font file truncated (bitmap)");
  }

  // Copy the bitmap data into the output structure.
  out.bitmap.assign(p, p + bitmap_bytes);
  p += bitmap_bytes;

  // remaining bytes are CharacterPosition records
  // keep going while we have at least one full struct of room in bytes left
  while (p + sizeof(CharacterPosition) <= bytes.data() + bytes.size()) {
    CharacterPosition cp;
    std::memcpy(&cp, p, sizeof(cp));
    out.positions.push_back(cp);
    p += sizeof(cp);
  }
  return out;
}

gfx::Surface FontLoader::build_atlas(const ParsedFont& pf, int scale,
                                     gfx::PaletteHandle palette) {
  // sum of glyph widths (skip invalid records)
  uint16_t width_px = 0;
  for (const auto& cp : pf.positions) {
    width_px += cp.width;
  }

  // space width hack copied from dos logic
  const uint16_t n_width = [&]() -> uint16_t {
    auto idx = static_cast<std::size_t>('n' - '!');
    if (idx < pf.positions.size()) {
      return pf.positions[idx].width / 2;
    }
    return pf.header.character_width / 2;
  }();

  width_px += n_width;

  const int atlas_w = width_px * scale;
  const int atlas_h = pf.header.character_height * scale;

  gfx::Surface atlas{atlas_w, atlas_h, palette, 0};

  ViewPoint p{0, 0};
  const uint8_t ink = 1;
  const auto row_bytes = pf.header.number_of_bytes_per_row;

  int byte_count = 0;

  // DOS format: 4 bits per pixel (nibbles)
  for (std::size_t i = 0; i < pf.bitmap.size(); ++i) {
    uint8_t byte = pf.bitmap[i];
    if (byte != 0 && byte_count < 10) {
      byte_count++;
    }
    for (int nybble = 1; nybble >= 0; --nybble) {
      uint8_t bits = (byte >> (nybble * 4)) & 0x0F;
      for (int k = 3; k >= 0; --k) {
        if (bits & (1u << k)) {
          for (int dy = 0; dy < scale; ++dy) {
            for (int dx = 0; dx < scale; ++dx) {
              atlas.put({p.x + dx, p.y + dy}, ink);
            }
          }
        }
        p.x += scale;
      }
    }
    if ((i + 1) % row_bytes == 0) { // next bitmap row
      p.x = 0;
      p.y += scale;
    }
  }

  return atlas;
}

std::unordered_map<uint32_t, Glyph>
FontLoader::build_glyph_table(const ParsedFont& pf, int scale) {
  std::unordered_map<uint32_t, Glyph> tbl;
  const int first = pf.header.first_character;
  const int last = pf.header.last_character;
  const size_t glyph_count = last - first + 1;

  const bool fixed = (pf.header.fixed_width_font_flag == 0x02);
  const uint16_t cell = pf.header.character_width;

  int valid_glyphs = 0;
  for (size_t idx = 0; idx < glyph_count; ++idx) {
    const auto& cp = pf.positions[idx];

    if (idx < 5 || (cp.start_pos != 0 && valid_glyphs < 5)) {
      if (cp.width > 0) {
        valid_glyphs++;
      }
    }

    if (cp.width == 0) {
      continue;
    }

    bool looks_like_blank = (cp.start_pos == 0) && // no bitmap
                            (cp.width != 0);       // but has an advance

    if (looks_like_blank) {
      uint16_t adv = (pf.header.fixed_width_font_flag == 0x02)
                         ? pf.header.character_width
                         : cp.width;

      tbl.emplace(
          U' ', Glyph{0, 0, static_cast<uint16_t>(adv * scale),
                      static_cast<uint16_t>(pf.header.character_height * scale),
                      U' '});
    }

    uint32_t ch = first + static_cast<uint8_t>(idx);

    uint16_t width = fixed ? cell : cp.width; // advance table handled later
    tbl.emplace(
        ch,
        Glyph{static_cast<uint16_t>(cp.start_pos * scale), // x inside bitmap
              0, static_cast<uint16_t>(width * scale),
              static_cast<uint16_t>(pf.header.character_height * scale), ch});
  }
  return tbl;
}

std::unique_ptr<Font> FontLoader::load(const std::string& filename) {
  FontFace face = FontManager::string_to_fontface(filename);

  const auto bytes = load_file_bytes(filename);

  // parse header, bitmap, positions
  const auto parsed = parse(bytes);

  const int scale = render_scale(face);

  auto atlas = build_atlas(parsed, scale, gfx::PaletteHandle{0});

  // Our font atlases has index 1 as the ink color,
  // disable palette translation so indices pass through unchanged
  atlas.disable_palette_translation();

  // glyph table
  auto glyphs = build_glyph_table(parsed, scale);

  // Calculate fixed character width for monospace fonts
  std::optional<int> fixed_width = std::nullopt;
  if (face == FontFace::SUPERSMA) {
    fixed_width = 8;
  }
  else if (face == FontFace::MINI) {
    fixed_width = 4;
  }
  else if (face == FontFace::CSTANDAR) {
    fixed_width = 10;
  }

  // package it all up into a Font
  FontData fd{
      .surface = std::move(atlas),
      .glyphs = std::move(glyphs),
      .line_height = parsed.header.character_height * scale,
      .fixed_char_width = fixed_width,
  };
  return std::make_unique<Font>(face, std::move(fd));
}

std::vector<uint8_t> FontLoader::load_file_bytes(std::string_view fname) {
  std::string filename{fname};
  if (filename == "MINI") {
    filename = "SUPERSMA";
  }

  std::string dos_path = std::string{"fonts/"} + filename + ".fnt";
  std::ifstream is = fileutils::open(dos_path);
  std::vector<uint8_t> data{std::istreambuf_iterator<char>{is},
                            std::istreambuf_iterator<char>{}};
  if (data.empty()) {
    fatal("Failed to load font: " + std::string(fname));
  }
  return data;
}
