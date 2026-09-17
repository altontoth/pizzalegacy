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
#include "resources/util/FileUtils.h"

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

  // A position and an advance for every character in range, plus one entry
  // for characters outside it.
  const std::size_t entries =
      out.header.last_character - out.header.first_character + 2;
  const bool proportional = out.header.fixed_width_font_flag != 0x02;
  std::size_t table_bytes = entries * sizeof(CharacterPosition);
  if (proportional) {
    table_bytes += entries * sizeof(uint16_t);
  }
  if (bytes.size() < sizeof(FontHeader) + bitmap_bytes + table_bytes) {
    fatal("font file truncated (tables)");
  }
  for (std::size_t i = 0; i < entries; ++i) {
    CharacterPosition cp;
    std::memcpy(&cp, p, sizeof(cp));
    out.positions.push_back(cp);
    p += sizeof(cp);
  }
  if (proportional) {
    for (std::size_t i = 0; i < entries; ++i) {
      uint16_t adv;
      std::memcpy(&adv, p, sizeof(adv));
      out.advances.push_back(adv);
      p += sizeof(adv);
    }
  }
  return out;
}

gfx::Surface FontLoader::build_atlas(const ParsedFont& pf, int scale,
                                     gfx::PaletteHandle palette) {
  uint16_t width_px = 0;
  for (const auto& cp : pf.positions) {
    width_px += cp.width;
  }

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
  const std::size_t glyph_count = last - first + 1;
  const bool fixed = (pf.header.fixed_width_font_flag == 0x02);

  for (std::size_t idx = 0; idx < glyph_count; ++idx) {
    const auto& cp = pf.positions[idx];
    const uint16_t advance =
        fixed ? pf.header.character_width : pf.advances[idx];
    const uint32_t ch = first + static_cast<uint8_t>(idx);
    tbl.emplace(
        ch,
        Glyph{
            .start_position_x = static_cast<uint16_t>(cp.start_pos * scale),
            .start_position_y = 0,
            .width = static_cast<uint16_t>(cp.width * scale),
            .height = static_cast<uint16_t>(pf.header.character_height * scale),
            .advance = static_cast<uint16_t>(advance * scale),
            .character = ch,
        });
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

  // package it all up into a Font
  FontData fd{
      .surface = std::move(atlas),
      .glyphs = std::move(glyphs),
      .line_height = parsed.header.character_height * scale,
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
