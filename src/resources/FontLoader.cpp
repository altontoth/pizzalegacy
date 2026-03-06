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

  // Check if this might be an Amiga hunk format (starts with 0x0000 03xx)
  bool is_amiga_hunk = (bytes.size() > 4 && bytes[0] == 0x00 &&
                        bytes[1] == 0x00 && bytes[2] == 0x03);

  ParsedFont out{};

  if (is_amiga_hunk) {
    // Amiga uses a completely different format with pointers
    return parse_amiga(bytes);
  }

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

FontLoader::ParsedFont FontLoader::parse_amiga(std::span<const uint8_t> bytes) {
  // Amiga TextFont structure starts at offset 0x6E
  // Pointers in the structure are relative to offset 0x20 (32 bytes into file)
  const size_t tf_offset = 0x6E;
  const size_t pointer_base = 0x20;

  if (bytes.size() < tf_offset + 30) {
    fatal("Amiga font file too small");
  }

  auto read_u16 = [&](size_t offset) -> uint16_t {
    return (bytes[offset] << 8) | bytes[offset + 1];
  };

  auto read_u32 = [&](size_t offset) -> uint32_t {
    return (bytes[offset] << 24) | (bytes[offset + 1] << 16) |
           (bytes[offset + 2] << 8) | bytes[offset + 3];
  };

  // Convert pointer from structure (relative to stripped file) to actual offset
  auto resolve_ptr = [&](uint32_t ptr) -> size_t { return ptr + pointer_base; };

  size_t pos = tf_offset;

  // Parse Amiga TextFont structure
  uint16_t ySize = read_u16(pos);
  pos += 2;
  uint8_t style = bytes[pos++];
  uint8_t flags = bytes[pos++];
  uint16_t xSize = read_u16(pos);
  pos += 2;
  uint16_t baseline = read_u16(pos);
  pos += 2;
  uint16_t boldSmear = read_u16(pos);
  pos += 2;
  uint16_t accessors = read_u16(pos);
  pos += 2;
  uint8_t loChar = bytes[pos++];
  uint8_t hiChar = bytes[pos++];
  uint32_t fontDataPointer = read_u32(pos);
  pos += 4;
  uint16_t modulo = read_u16(pos);
  pos += 2;
  uint32_t locationDataPointer = read_u32(pos);
  pos += 4;
  uint32_t spacingDataPointer = read_u32(pos);
  pos += 4;
  uint32_t kerningDataPointer = read_u32(pos);
  pos += 4;

  // Build our FontHeader structure
  ParsedFont out{};
  out.is_amiga = true;
  out.header.character_height = ySize;
  out.header.padding = 0;
  // Amiga: bit 5 of flags indicates proportional (0x20 = bit 5)
  out.header.fixed_width_font_flag = (flags & 0x20) ? 0x62 : 0x02;
  out.header.character_width = xSize;
  out.header.baseline_height = baseline;
  out.header.first_character = loChar;
  out.header.last_character = hiChar;
  out.header.number_of_bytes_per_row = modulo;

  // Extract bitmap data
  const size_t bitmap_offset = resolve_ptr(fontDataPointer);
  const size_t bitmap_size = static_cast<const size_t>(modulo * ySize);
  if (bitmap_offset + bitmap_size > bytes.size()) {
    fatal("Amiga font bitmap pointer out of range");
  }

  out.bitmap.assign(bytes.begin() + bitmap_offset,
                    bytes.begin() + bitmap_offset + bitmap_size);

  // Extract character location data
  const size_t location_offset = resolve_ptr(locationDataPointer);
  // +1 for range, +1 for "notdef":
  const size_t char_count = hiChar - loChar + 2;
  // Each entry is 2x uint16_t:
  const size_t location_size = char_count * 4;

  if (location_offset + location_size > bytes.size()) {
    fatal("Amiga font location pointer out of range");
  }

  // Extract character positions
  // The array seems to start before loChar - we need to find where loChar
  // actually starts
  uint16_t first_start_pos = read_u16(location_offset);
  uint16_t char_width = read_u16(location_offset + 2);

  // Calculate how many characters come before loChar based on start position
  size_t skip_count = (char_width > 0) ? (first_start_pos / char_width) : 0;

  // Extract the positions we need (loChar..hiChar)
  const size_t chars_needed = hiChar - loChar + 1;
  for (size_t i = 0; i < chars_needed; ++i) {
    size_t loc_pos = location_offset + ((skip_count + i) * 4);
    CharacterPosition cp;
    cp.start_pos = read_u16(loc_pos);
    cp.width = read_u16(loc_pos + 2);
    out.positions.push_back(cp);
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

  if (pf.is_amiga) {
    // Amiga format: 1 bit per pixel
    for (std::size_t i = 0; i < pf.bitmap.size(); ++i) {
      uint8_t byte = pf.bitmap[i];
      if (byte != 0 && byte_count < 10) {
        byte_count++;
      }
      // Process all 8 bits in the byte
      for (int bit = 7; bit >= 0; --bit) {
        if (byte & (1u << bit)) {
          for (int dy = 0; dy < scale; ++dy) {
            for (int dx = 0; dx < scale; ++dx) {
              atlas.put({p.x + dx, p.y + dy}, ink);
            }
          }
        }
        p.x += scale;
      }
      // next bitmap row:
      if ((i + 1) % row_bytes == 0) {
        p.x = 0;
        p.y += scale;
      }
    }
  }
  else {
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
  const int cell = pf.header.character_width;

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

std::string FontLoader::dos_to_amiga_font_name(std::string_view dos_name) {
  static const std::unordered_map<std::string_view, std::string_view> name_map =
      {
          {"SUPERSMA", "piz-supersmall"}, {"MINI", "piz-supersmall"},
          {"RUBY", "piz-ruby"},           {"PIZ-NUMB", "piz-number"},
          {"MENU", "piz-Menu"},           {"CSTANDAR", "piz-CStandard"},
          {"ASTANDAR", "piz-AStandard"},
      };

  if (auto it = name_map.find(dos_name); it != name_map.end()) {
    return std::string(it->second);
  }
  return std::string(dos_name);
}

std::optional<std::string>
FontLoader::read_amiga_font_path(const std::string& font_file) {
  if (!fileutils::fexists(font_file)) {
    return std::nullopt;
  }

  auto is = fileutils::open(font_file);
  if (!is) {
    return std::nullopt;
  }

  // Skip first 4 bytes (header)
  is.seekg(4);

  // Read path string (null-terminated, max 256 chars)
  std::string path;
  char ch;
  while (is.get(ch) && ch != '\0' && path.size() < 256) {
    path += ch;
  }

  if (path.empty()) {
    return std::nullopt;
  }

  return path;
}

std::optional<std::vector<uint8_t>>
FontLoader::try_load_amiga_font(std::string_view fname) {
  std::string dos_name{fname};
  if (dos_name == "MINI") {
    dos_name = "SUPERSMA";
  }

  std::string amiga_name = dos_to_amiga_font_name(dos_name);
  std::string font_descriptor = "fonts/" + amiga_name + ".font";

  // Try to read the .font descriptor file
  auto path_opt = read_amiga_font_path(font_descriptor);
  if (!path_opt) {
    return std::nullopt;
  }

  // Load the actual font data file
  std::string font_data_path = "fonts/" + *path_opt;

  if (!fileutils::fexists(font_data_path)) {
    return std::nullopt;
  }

  // Logger::debug("Loading Amiga font from: {}", font_data_path);

  auto is = fileutils::open(font_data_path);
  std::vector<uint8_t> data{std::istreambuf_iterator<char>{is},
                            std::istreambuf_iterator<char>{}};
  return data;
}

std::vector<uint8_t> FontLoader::load_file_bytes(std::string_view fname) {
  std::string filename{fname};
  if (filename == "MINI") {
    filename = "SUPERSMA";
  }

  // Try DOS format first (check if file exists before opening)
  std::string dos_path = std::string{"fonts/"} + filename + ".fnt";
  if (fileutils::fexists(dos_path)) {
    std::ifstream is = fileutils::open(dos_path);
    std::vector<uint8_t> data{std::istreambuf_iterator<char>{is},
                              std::istreambuf_iterator<char>{}};
    if (!data.empty()) {
      // Logger::debug("Loaded DOS font: {}", filename);
      return data;
    }
  }

  // Try Amiga format
  auto amiga_data = try_load_amiga_font(fname);
  if (amiga_data) {
    // Logger::debug("Loaded Amiga font: {}", std::string(fname));
    return *amiga_data;
  }

  fatal("Failed to load font: " + std::string(fname));
}
