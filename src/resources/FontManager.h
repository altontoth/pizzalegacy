/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <string>
#include <vector>
#include "Font.h"
#include "FontLoader.h"
#include "ResourceManager.h"
#include "engine/geom/Spaces.h"
#include "gfx/Surface.h"
#include "resources/FontTypes.h"

using gfx::Surface;

inline std::string_view to_string(FontFace face) {
  switch (face) {
  case FontFace::ASTANDAR:
    return "ASTANDAR";
  case FontFace::CSTANDAR:
    return "CSTANDAR";
  case FontFace::MENU:
    return "MENU";
  case FontFace::PIZ_NUMB:
    return "PIZ-NUMB";
  case FontFace::RUBY:
    return "RUBY";
  case FontFace::SUPERSMA:
    return "SUPERSMA";
  case FontFace::MINI:
    return "MINI";
  default:
    return "UNKNOWN";
  }
}

class FontManager : public ResourceManager<Font, FontLoader> {
public:
  // Inherit constructor
  using ResourceManager<Font, FontLoader>::ResourceManager;

  // Custom get() method that accepts FontFace instead of std::string
  Font& get(FontFace face);
  RenderedTextMetrics render_line_onto(Surface& target_surface,
                                       std::string_view text, ViewPoint p,
                                       FontFace face, uint8_t color,
                                       AlignX align = AlignX::Left);

  RenderedTextMetrics render_line_onto(Surface& surface, std::string_view text,
                                       ViewPoint p, FontFace face,
                                       const TextStyle& style = {});

  RenderedTextMetrics
  render_text_onto(Surface& surface, std::string_view text, ViewPoint p,
                   FontFace face, const TextStyle& style = {},
                   std::optional<FlowColumns> flow = std::nullopt);

  RenderedTextMetrics
  render_text_onto(Surface& surface, std::string_view text, ViewPoint p,
                   FontFace face, uint8_t color, AlignX align = AlignX::Left,
                   std::optional<FlowColumns> flow = std::nullopt);

  RenderedTextMetrics render_into_rect(Surface& surface, std::string_view text,
                                       ViewRect rect, FontFace face,
                                       const TextStyle& style = {});

  RenderedTextMetrics size_text(std::string_view text, FontFace face,
                                const TextStyle& style = {});
  RenderedTextMetrics size_text(std::string_view text, FontFace face,
                                int max_width, const TextStyle& style = {});

  int count_wrapped_lines(std::string_view text, FontFace face, int max_width);

  static std::vector<FontFace> font_list();
  static FontFace string_to_fontface(std::string_view file);

private:
  static const std::unordered_map<std::string, FontFace> font_map_;
};
