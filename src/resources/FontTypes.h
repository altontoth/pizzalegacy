/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include "engine/geom/PxOffset.h"
#include "engine/gfx/ColorIndex.h"
#include "engine/gfx/Surface.h"
#include "engine/ui/TextAnchor.h"

using gfx::Surface;

enum class FontFace {
  ASTANDAR = 0,
  CSTANDAR,
  MENU,
  PIZ_NUMB,
  RUBY,
  SUPERSMA,
  MINI,
};

struct Glyph {
  uint16_t start_position_x{0};
  uint16_t start_position_y{0};
  uint16_t width{0}; // ink columns
  uint16_t height{0};
  uint16_t advance{0}; // pen movement after the glyph
  uint32_t character{0};
};

struct FontData {
  Surface surface;
  std::unordered_map<uint32_t, Glyph>
      glyphs;         // Mapping from character to glyph data
  int line_height{0}; // Height of a line in this font face
};

struct ShadowStyle {
  PxOffset offset{2, 2};
  gfx::ColorIndex color{1};
};

struct TextStyle {
  gfx::ColorIndex color{240};
  TextAnchor align{TextAnchor::Start};
  std::optional<ShadowStyle> shadow = std::nullopt;
  std::optional<int> character_spacing = std::nullopt; // pixels between chars
  int line_spacing{0}; // extra pixels added between lines (multi-line only)
  bool soft_hyphens{false};      // treat letter-dash-letter as break hints
  bool reflow_pipe_lines{false}; // re-wrap if a '|' line is too wide
};
