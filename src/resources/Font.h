/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>
#include <vector>
#include "FontTypes.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/TextAnchor.h"
#include "gfx/Surface.h"

using gfx::Surface;

struct RenderedTextMetrics {
  int width = 0;
  int height = 0;
  int render_x = 0;
};

// Configuration for text that flows around an image
struct FlowColumns {
  int cutoff_y; // Y in the target surface where the left margin switches
  int left_margin_above;
  int left_margin_below;
  int right_margin; // Applies throughout
};

// Line with its Y position for flowing text
struct FlowLine {
  std::string_view text;
  int baseline_y; // Y position relative to start
};

class Font {
public:
  Font(FontFace face, FontData data);
  ~Font() = default;

  // Move constructor and assignment
  Font(Font&&) noexcept = default;
  Font& operator=(Font&&) noexcept = default;

  // Delete copy constructor and assignment
  Font(const Font&) = delete;
  Font& operator=(const Font&) = delete;

  RenderedTextMetrics render_line_onto(Surface& target_surface,
                                       std::string_view text, ViewPoint p,
                                       const TextStyle& style = {});
  RenderedTextMetrics
  render_text_onto(Surface& surface, std::string_view text, ViewPoint p,
                   const TextStyle& style = {},
                   std::optional<FlowColumns> flow = std::nullopt);
  RenderedTextMetrics render_into_rect(Surface& surface, std::string_view text,
                                       ViewRect rect,
                                       const TextStyle& style = {});
  RenderedTextMetrics size_text(std::string_view text,
                                const TextStyle& style = {});
  RenderedTextMetrics size_text(std::string_view text, int max_width,
                                const TextStyle& style = {});
  int count_wrapped_lines(std::string_view text, int max_width,
                          const TextStyle& style = {});

  // Expose text wrapping for pre-computing line breaks
  std::vector<std::string> split_and_wrap_text(std::string_view text,
                                               int max_width,
                                               const TextStyle& style = {});

  // The original's text files mark their line breaks with '|'.
  static std::vector<std::string_view> split_by_pipe(std::string_view text);

  // Soft hyphen analysis: dashes between letters mark allowed break points
  struct BreakableWord {
    std::string clean_text;              // text without soft hyphens
    std::vector<size_t> break_positions; // indices where breaks are allowed
  };
  static BreakableWord analyze_word_breaks(std::string_view word);

private:
  [[nodiscard]] int line_spacing() const;
  [[nodiscard]] int get_character_spacing(const TextStyle& style) const;
  uint32_t substitute_character(uint32_t codepoint) const;
  uint32_t remap_character(uint32_t codepoint) const;
  // The glyph for a codepoint, falling back to its substitute. Null when
  // neither is in the font.
  [[nodiscard]] const Glyph* find_glyph(uint32_t codepoint) const;
  // Excludes inter-character spacing.
  [[nodiscard]] int advance_width(const Glyph& glyph) const;
  RenderedTextMetrics render_fixed_width(Surface& surface,
                                         std::string_view text, ViewPoint pos,
                                         int max_width, const TextStyle& style);
  FontFace face_;
  FontData data_;

  // Fixed-width wrapping
  std::vector<std::string> word_wrap_line(std::string_view input_line,
                                          int max_width,
                                          const TextStyle& style = {});

  // Variable-width wrapping (for text flow)
  std::vector<FlowLine> word_wrap_with_flow(std::string_view input_line,
                                            int start_y,
                                            const FlowColumns& flow,
                                            const TextStyle& style = {});
  std::vector<FlowLine> split_and_wrap_with_flow(std::string_view text,
                                                 int start_y,
                                                 const FlowColumns& flow,
                                                 const TextStyle& style = {});
};
