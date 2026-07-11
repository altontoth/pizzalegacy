/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "FontManager.h"
#include "engine/geom/Spaces.h"

const std::unordered_map<std::string, FontFace> FontManager::font_map_ = {
    {"CSTANDAR", FontFace::CSTANDAR}, {"ASTANDAR", FontFace::ASTANDAR},
    {"MENU", FontFace::MENU},         {"SUPERSMA", FontFace::SUPERSMA},
    {"RUBY", FontFace::RUBY},         {"PIZ-NUMB", FontFace::PIZ_NUMB},
    {"MINI", FontFace::MINI}};

Font& FontManager::get(FontFace face) {
  return ResourceManager::get(to_string(face));
}

RenderedTextMetrics FontManager::render_line_onto(Surface& surface,
                                                  std::string_view text,
                                                  ViewPoint p, FontFace face,
                                                  uint8_t color, AlignX align) {
  return get(face).render_line_onto(
      surface, text, p,
      TextStyle{.color = color, .align = align, .shadow = {}});
}

RenderedTextMetrics
FontManager::render_text_onto(Surface& surface, std::string_view text,
                              ViewPoint p, FontFace face, uint8_t color,
                              AlignX align, std::optional<FlowColumns> flow) {
  return get(face).render_text_onto(
      surface, text, p, TextStyle{.color = color, .align = align, .shadow = {}},
      flow);
}

RenderedTextMetrics FontManager::render_line_onto(Surface& surface,
                                                  std::string_view text,
                                                  ViewPoint p, FontFace face,
                                                  const TextStyle& style) {
  return get(face).render_line_onto(surface, text, p, style);
}

RenderedTextMetrics FontManager::render_text_onto(
    Surface& surface, std::string_view text, ViewPoint p, FontFace face,
    const TextStyle& style, std::optional<FlowColumns> flow) {
  return get(face).render_text_onto(surface, text, p, style, flow);
}

RenderedTextMetrics FontManager::render_into_rect(Surface& surface,
                                                  std::string_view text,
                                                  ViewRect rect, FontFace face,
                                                  const TextStyle& style) {
  return get(face).render_into_rect(surface, text, rect, style);
}

RenderedTextMetrics FontManager::size_text(std::string_view text, FontFace face,
                                           const TextStyle& style) {
  return get(face).size_text(text, style);
}

RenderedTextMetrics FontManager::size_text(std::string_view text, FontFace face,
                                           int max_width,
                                           const TextStyle& style) {
  return get(face).size_text(text, max_width, style);
}

int FontManager::count_wrapped_lines(std::string_view text, FontFace face,
                                     int max_width) {
  return get(face).count_wrapped_lines(text, max_width);
}

std::vector<FontFace> FontManager::font_list() {
  return std::vector<FontFace>{
      FontFace::ASTANDAR, FontFace::CSTANDAR, FontFace::MENU,
      FontFace::PIZ_NUMB, FontFace::RUBY,     FontFace::SUPERSMA,
      FontFace::MINI,
  };
}

FontFace FontManager::string_to_fontface(std::string_view filename) {
  if (auto it = font_map_.find(std::string(filename)); it != font_map_.end()) {
    return it->second;
  }

  Logger::debug("Can't find font face for filename {}", filename);
  throw std::invalid_argument("Invalid font filename, can't find it");
}
