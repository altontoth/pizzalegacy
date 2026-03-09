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
#include "engine/ui/Alignment.h"
#include "gfx/Surface.h"

using gfx::Surface;

static constexpr uint8_t BLACK = 1;
static constexpr uint8_t WHITE = 255;
static constexpr uint8_t YELLOW = 155;
static constexpr uint8_t DISTRICT_WHITE = 15;
static constexpr uint8_t TEXT_DARK_GRAY = 244;
static constexpr uint8_t TEXT_MEDIUM_GRAY = 248;
static constexpr uint8_t SCROLL_TABLE_BODY_TEXT = 13;
static constexpr uint8_t RED = 32;
static constexpr uint8_t TEXT_DARK_BROWN = 5;
static constexpr uint8_t GRAY = 250;
static constexpr uint8_t SCROLL_TABLE_HEADER_TEXT = 12;
static constexpr uint8_t TEXT_DARKER_BROWN = 6;
static constexpr uint8_t TEXT_BEIGE = 14;
static constexpr uint8_t TEXT_LIGHT_GRAY = 251;
static constexpr uint8_t BEIGE = 13;
static constexpr uint8_t BRIGHTGREEN = 26;
static constexpr uint8_t DARKGRAY = 247;
static constexpr uint8_t GREEN = 96;

struct RenderedTextMetrics {
  int width = 0;
  int height = 0;
  int render_x = 0;
};

// Configuration for text that flows around an image
struct FlowColumns {
  int cutoff_y; // Y threshold (relative to start) where we switch from narrow
                // to wide
  int left_narrow; // Left margin while Y < cutoff_y (next to picture)
  int left_wide;   // Left margin after Y >= cutoff_y (below picture)
  int right;       // Right margin (constant throughout)
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
  int count_wrapped_lines(std::string_view text, int max_width,
                          const TextStyle& style = {});
  uint32_t substitute_character(uint32_t codepoint);
  uint32_t remap_character(uint32_t codepoint);

  // Expose text wrapping for pre-computing line breaks
  std::vector<std::string_view>
  split_and_wrap_text(std::string_view text, int max_width,
                      const TextStyle& style = {});

private:
  [[nodiscard]] int line_spacing() const;
  [[nodiscard]] int get_character_spacing(const TextStyle& style) const;
  RenderedTextMetrics render_fixed_width(Surface& surface,
                                         std::string_view text, ViewPoint pos,
                                         int max_width, const TextStyle& style);
  FontFace face_;
  FontData data_;

  // Soft hyphen support (shared helpers)
  struct BreakableWord {
    std::string clean_text;              // Text with soft hyphens removed
    std::vector<size_t> break_positions; // Positions where breaks are allowed
  };
  BreakableWord analyze_word_breaks(std::string_view word);

  // Fixed-width wrapping (existing)
  static std::vector<std::string_view> split_by_pipe(std::string_view text);
  std::vector<std::string_view> word_wrap_line(std::string_view input_line,
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
