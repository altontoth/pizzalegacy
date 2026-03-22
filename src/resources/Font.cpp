/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "Font.h"

#include <algorithm>
#include <cstdlib>
#include <set>
#include <string>
#include <utfcpp/utf8.h>
#include "engine/Fatal.h"
#include "engine/ui/Alignment.h"
#include "resources/FontTypes.h"

constexpr int SPACE_BETWEEN_CHARACTERS = 2;
static constexpr uint8_t TRANSPARENT_INDEX = 0;

const std::set<uint32_t> unicode_whitespace = {
    0x0009, // Tab
    0x0020, // Space
    0x00A0, // No-Break Space
};

Font::Font(FontFace face, FontData data)
    : face_(face), data_(std::move(data)) {}

std::vector<std::string_view> Font::split_by_pipe(std::string_view text) {
  std::vector<std::string_view> lines;
  std::size_t start_pos = 0;

  while (true) {
    std::size_t found_pos = text.find('|', start_pos);
    if (found_pos == std::string::npos) {
      lines.push_back(text.substr(start_pos));
      break;
    }
    lines.push_back(text.substr(start_pos, found_pos - start_pos));
    start_pos = found_pos + 1;
  }
  return lines;
}

/*
 * We interpret '|' as newline(s), so we split on '|', then render each line
 * with render_line_onto at the appropriate y-offset.
 *
 * If flow is provided, text flows around an image with variable column widths.
 */
RenderedTextMetrics Font::render_text_onto(Surface& surface,
                                           std::string_view text, ViewPoint pos,
                                           const TextStyle& style,
                                           std::optional<FlowColumns> flow) {
  RenderedTextMetrics metrics;
  // Quick bail-out if text is empty
  if (text.empty()) {
    return {};
  }

  // Flow text path: variable width wrapping
  if (flow.has_value()) {
    // Process soft hyphens in the entire text
    std::string cleaned_text;
    cleaned_text.reserve(text.size());

    for (size_t i = 0; i < text.size(); ++i) {
      char ch = text[i];

      // Check if this is a soft hyphen
      if (ch == '-' && i > 0 && i < text.size() - 1) {
        char before = text[i - 1];
        char after = text[i + 1];

        // Soft hyphen if surrounded by letters
        if (std::isalpha(static_cast<unsigned char>(before)) &&
            std::isalpha(static_cast<unsigned char>(after))) {
          // Skip the soft hyphen
          continue;
        }
      }

      // Regular character or non-soft-hyphen dash
      cleaned_text += ch;
    }

    // Get lines with their Y positions
    auto flow_lines =
        split_and_wrap_with_flow(cleaned_text, pos.y, *flow, style);

    int total_height = 0;
    int max_width = 0;
    int initial_x = -1;

    // Render each line at its designated position
    for (const auto& flow_line : flow_lines) {
      // Determine X position based on Y position
      int x = (flow_line.baseline_y < flow->cutoff_y) ? flow->left_narrow
                                                      : flow->left_wide;

      auto line_metrics = render_line_onto(
          surface, flow_line.text, ViewPoint{x, flow_line.baseline_y}, style);

      max_width = std::max(line_metrics.width, max_width);
      if (initial_x == -1) {
        initial_x = line_metrics.render_x;
      }
    }

    // Calculate total height
    if (!flow_lines.empty()) {
      total_height = flow_lines.back().baseline_y - pos.y + line_spacing() +
                     style.line_spacing;
    }

    if (style.shadow && style.shadow->offset.dy > 0) {
      total_height += style.shadow->offset.dy;
    }

    metrics.width = max_width;
    metrics.height = total_height;
    metrics.render_x = initial_x;
    return metrics;
  }

  // Standard path: fixed width wrapping
  // The available wrap width depends on alignment: for Left, text starts at
  // pos.x and extends right. For CenterPoint, text extends both ways from
  // pos.x. For RightEdge, text extends left from pos.x.
  int available_width;
  if (style.align == AlignX::CenterPoint) {
    available_width = 2 * std::min(pos.x, surface.width() - pos.x);
  }
  else if (style.align == AlignX::RightEdge) {
    available_width = pos.x;
  }
  else {
    available_width = surface.width() - pos.x;
  }
  if (style.shadow && style.shadow->offset.dx > 0) {
    available_width -= style.shadow->offset.dx;
  }
  return render_fixed_width(surface, text, pos, available_width, style);
}

RenderedTextMetrics Font::size_text(std::string_view text,
                                    const TextStyle& style) {
  RenderedTextMetrics metrics;

  auto it = utf8::iterator(text.begin(), text.begin(), text.end());
  auto it_end = utf8::iterator(text.end(), text.begin(), text.end());

  while (it != it_end) {
    uint32_t cp = remap_character(*it);
    auto glyph_it = data_.glyphs.find(cp);
    if (glyph_it == data_.glyphs.end()) {
      uint32_t substitute_codepoint = substitute_character(cp);
      glyph_it = data_.glyphs.find(substitute_codepoint);
    }
    if (glyph_it != data_.glyphs.end()) {
      // Use fixed character width for monospace fonts, otherwise use actual
      // glyph width
      int char_width = data_.fixed_char_width.value_or(glyph_it->second.width);

      // We need one fewer space between characters than number of
      // characters in our string (3 chars == 2 spaces, etc):
      auto next_it = it;
      next_it++;
      if (next_it != it_end) {
        metrics.width += get_character_spacing(style);
      }
      metrics.width += char_width;
      metrics.height = std::max(metrics.height, (int)glyph_it->second.height);
    }
    it++;
  }

  // Inflate width/height to include the shadow's offset if enabled.
  if (style.shadow) {
    // Horizontal union: width grows by the positive part of dx
    if (style.shadow->offset.dx > 0) {
      metrics.width += style.shadow->offset.dx;
    }
    // Vertical union: height grows by the positive part of dy
    if (style.shadow->offset.dy > 0) {
      metrics.height += style.shadow->offset.dy;
    }
  }

  return metrics;
}

int Font::count_wrapped_lines(std::string_view text, int max_width,
                              const TextStyle& style) {
  auto lines = split_and_wrap_text(text, max_width, style);
  return static_cast<int>(lines.size());
}

RenderedTextMetrics Font::render_line_onto(Surface& target,
                                           std::string_view text, ViewPoint pos,
                                           const TextStyle& style) {
  RenderedTextMetrics out{};
  auto size = size_text(text, style);

  ViewPoint origin;
  if (style.align == AlignX::RightEdge) {
    // For RightEdge: pos.x is the right edge of the text
    origin.x = pos.x - size.width;
    origin.y = pos.y;
  }
  else if (style.align == AlignX::CenterPoint) {
    // For CenterPoint: pos.x is the center of the text
    origin.x = pos.x - (size.width / 2);
    origin.y = pos.y;
  }
  else {
    // Legacy alignment: create container rect and align within it
    ViewRect r{pos, target.size()};
    origin =
        views::ui::align_in_rect(r, {size.width, size.height}, style.align);
  }
  out.render_x = origin.x;

  // We'll render twice if shadow is enabled; first shadow then foreground
  auto render_pass = [&](ViewPoint pos, uint8_t color) {
    auto it = utf8::iterator(text.begin(), text.begin(), text.end());
    auto it_end = utf8::iterator(text.end(), text.begin(), text.end());

    ViewPoint cur = pos;

    while (it != it_end) {
      uint32_t cp = remap_character(*it);
      auto g_it = data_.glyphs.find(cp);
      if (g_it == data_.glyphs.end()) {
        cp = substitute_character(cp);
        g_it = data_.glyphs.find(cp);
      }
      if (g_it != data_.glyphs.end()) {
        const Glyph& g = g_it->second;

        // skip blit if this glyph has no pixels
        if (g.start_position_x != 0 || cp != U' ') {
          // Create temp surface for this glyph to avoid replacing color index 1
          // in the target surface (which may have index 1 pixels from the
          // background)
          gfx::Surface temp_glyph(g.width, g.height, data_.surface.palette());

          ViewRect src{g.start_position_x, g.start_position_y, g.width,
                       g.height};
          temp_glyph.blit(data_.surface, src, {0, 0}, TRANSPARENT_INDEX);
          temp_glyph.replace_colors({1}, color);

          target.blit(temp_glyph, {{0, 0}, temp_glyph.size()}, {cur.x, pos.y},
                      TRANSPARENT_INDEX);
        }

        // Use fixed character width for monospace fonts, otherwise use actual
        // glyph width
        int advance_width = data_.fixed_char_width.value_or(g.width);
        cur.x += advance_width;

        // Only add spacing if NOT the last character
        auto next_it = it;
        ++next_it;
        if (next_it != it_end) {
          cur.x += get_character_spacing(style);
        }
      }
      ++it;
    }
  };

  // Shadow pass first (if enabled), then the foreground.
  if (style.shadow) {
    ViewPoint shadow_origin = origin + style.shadow->offset;
    render_pass(shadow_origin, style.shadow->color.get());
  }

  // Foreground pass
  render_pass(origin, style.color.get());

  out.width = size.width;
  out.height = size.height;
  return out;
}

uint32_t Font::remap_character(uint32_t codepoint) {
  if (face_ == FontFace::SUPERSMA || face_ == FontFace::MINI) {
    if (codepoint == '\\') {
      return '/';
    }
  }
  return codepoint;
}

uint32_t Font::substitute_character(uint32_t codepoint) {
  if (codepoint == 0) {
    return '\0';
  }
  if (face_ == FontFace::PIZ_NUMB) {
    if (codepoint == 'l') {
      return 'I';
    }
    /*
    else {
      Logger::debug("pn I don't have a {}", codepoint);
      return '?';
    }
    */
  }

  if (face_ == FontFace::SUPERSMA || face_ == FontFace::MINI ||
      face_ == FontFace::CSTANDAR) {
    if (codepoint >= 'a' && codepoint <= 'z') {
      return codepoint - ('a' - 'A');
    }
    if (face_ == FontFace::MINI && codepoint == '_') {
      return '.';
    }
    if (face_ == FontFace::MINI && codepoint == '{') {
      return '(';
    }
    if (face_ == FontFace::MINI && codepoint == '}') {
      return ')';
    }
    if (face_ == FontFace::MINI && codepoint == '[') {
      return '(';
    }
    if (face_ == FontFace::MINI && codepoint == ']') {
      return ')';
    }
    // Backslash remapping is handled by remap_character() before glyph lookup
    /*
    else {
      Logger::debug("ss mn cs  I don't have a {}", codepoint);
      return '?';
    }
    */
  }

  return (uint32_t)'?';
}

// Strips dashes between letters and records break positions.
// e.g. "cer-ti-fi-cates" -> clean_text="certificates", break_positions={3,5,7}
Font::BreakableWord Font::analyze_word_breaks(std::string_view word) {
  BreakableWord result;
  result.clean_text.reserve(word.size());

  for (size_t i = 0; i < word.size(); ++i) {
    char ch = word[i];
    if (ch == '-' && i > 0 && i < word.size() - 1) {
      char before = word[i - 1];
      char after = word[i + 1];
      if (std::isalpha(static_cast<unsigned char>(before)) &&
          std::isalpha(static_cast<unsigned char>(after))) {
        result.break_positions.push_back(result.clean_text.size());
        continue;
      }
    }
    result.clean_text += ch;
  }

  return result;
}

std::vector<std::string> Font::word_wrap_line(std::string_view input_line,
                                              int max_width,
                                              const TextStyle& style) {
  std::vector<std::string> out;

  const auto n = input_line.size();
  std::size_t i = 0;

  while (i < n &&
         (std::isspace(static_cast<unsigned char>(input_line[i])) != 0)) {
    ++i;
  }

  const int boundary = get_character_spacing(style);

  TextStyle wrap_style = style;
  wrap_style.shadow = std::nullopt;

  std::string current_line;
  int current_w = 0;

  while (i < n) {
    // Find next whitespace-delimited word
    const std::size_t w_begin = i;
    while (i < n &&
           (std::isspace(static_cast<unsigned char>(input_line[i])) == 0)) {
      ++i;
    }
    const std::size_t w_end = i;

    // Grab trailing whitespace
    const std::size_t ws_begin = w_end;
    while (i < n &&
           (std::isspace(static_cast<unsigned char>(input_line[i])) != 0)) {
      ++i;
    }
    const std::size_t ws_end = i;

    auto raw_word = input_line.substr(w_begin, w_end - w_begin);
    auto ws = input_line.substr(ws_begin, ws_end - ws_begin);
    auto bw = analyze_word_breaks(raw_word);
    const int w_width = size_text(bw.clean_text, wrap_style).width;
    const int ws_width = ws.empty() ? 0 : size_text(ws, wrap_style).width;

    // Does the clean word fit on the current line?
    int needed = (current_w == 0) ? w_width : current_w + boundary + w_width;
    if (needed <= max_width) {
      // Fits whole
      if (!current_line.empty()) {
        current_line += ' ';
      }
      current_line += bw.clean_text;
      current_w = needed;
      if (ws_width > 0) {
        current_w += boundary + ws_width;
      }
      continue;
    }

    // Doesn't fit, try breaking at soft-hyphen positions
    if (!bw.break_positions.empty()) {
      int avail = max_width - current_w;
      if (!current_line.empty()) {
        avail -= boundary;
      }

      // find the best break that fits (with trailing dash)
      std::string dash_str("-");
      int dash_w = size_text(dash_str, wrap_style).width;
      int best_break = -1;

      for (auto pos : bw.break_positions) {
        auto fragment = bw.clean_text.substr(0, pos);
        int frag_w = size_text(fragment, wrap_style).width + dash_w;
        if (frag_w <= avail) {
          best_break = static_cast<int>(pos);
        }
        else {
          break;
        }
      }

      if (best_break >= 0) {
        // emit current line + first part with dash
        if (!current_line.empty()) {
          current_line += ' ';
        }
        current_line += bw.clean_text.substr(0, best_break) + '-';
        out.push_back(std::move(current_line));
        current_line.clear();

        // remainder becomes the start of the next line
        auto remainder = bw.clean_text.substr(best_break);
        current_w = size_text(remainder, wrap_style).width;
        current_line = std::move(remainder);
        if (ws_width > 0) {
          current_w += boundary + ws_width;
        }
        continue;
      }
    }

    // No soft-hyphen break worked, fall back to whole-word wrap
    if (!current_line.empty()) {
      out.push_back(std::move(current_line));
      current_line.clear();
      current_w = 0;
    }
    current_line = bw.clean_text;
    current_w = w_width;
    if (ws_width > 0) {
      current_w += boundary + ws_width;
    }
  }

  if (!current_line.empty()) {
    out.push_back(std::move(current_line));
  }

  return out;
}

std::vector<std::string> Font::split_and_wrap_text(std::string_view text,
                                                   int max_width,
                                                   const TextStyle& style) {
  auto pipe_lines = split_by_pipe(text);
  std::vector<std::string> final_lines;

  for (std::string_view line : pipe_lines) {
    auto wrapped = word_wrap_line(line, max_width, style);
    for (auto& w : wrapped) {
      final_lines.push_back(std::move(w));
    }
  }
  return final_lines;
}

// Word wrap with variable width based on Y position (for text flowing around
// images-input_line should already have soft hyphens processed)
std::vector<FlowLine> Font::word_wrap_with_flow(std::string_view input_line,
                                                int start_y,
                                                const FlowColumns& flow,
                                                const TextStyle& style) {
  std::vector<FlowLine> out;

  const auto n = input_line.size();
  std::size_t i = 0;

  // Skip leading whitespace
  while (i < n &&
         (std::isspace(static_cast<unsigned char>(input_line[i])) != 0)) {
    ++i;
  }

  // See comment in word_wrap_line: boundary spacing for joining fragments
  const int boundary = get_character_spacing(style);

  TextStyle wrap_style = style;
  wrap_style.shadow = std::nullopt;

  std::size_t line_start = i;
  std::size_t last_word_end = i;
  int current_w = 0;
  int current_y = start_y;
  int vert_spacing = line_spacing() + style.line_spacing;

  while (i < n) {
    // Determine current max width based on Y position
    int left_margin =
        (current_y < flow.cutoff_y) ? flow.left_narrow : flow.left_wide;
    int max_width = flow.right - left_margin;

    // Find next word [w_begin, w_end)
    const std::size_t w_begin = i;
    while (i < n &&
           (std::isspace(static_cast<unsigned char>(input_line[i])) == 0)) {
      ++i;
    }
    const std::size_t w_end = i;

    // Measure word
    const auto word_view = input_line.substr(w_begin, w_end - w_begin);
    const int w_width = size_text(word_view, wrap_style).width;

    // Measure trailing whitespace
    const std::size_t ws_begin = i;
    while (i < n &&
           (std::isspace(static_cast<unsigned char>(input_line[i])) != 0)) {
      ++i;
    }
    const std::size_t ws_end = i;
    const auto ws_view = input_line.substr(ws_begin, ws_end - ws_begin);
    const int ws_width =
        ws_view.empty() ? 0 : size_text(ws_view, wrap_style).width;

    // If this word overflows the current line, emit and start fresh
    if (current_w > 0 && current_w + boundary + w_width > max_width) {
      if (last_word_end > line_start) {
        out.push_back(
            {input_line.substr(line_start, last_word_end - line_start),
             current_y});
      }
      current_y += vert_spacing;
      line_start = w_begin;
      current_w = 0;
    }

    // Accumulate: word (with boundary spacing if joining to existing content)
    current_w += (current_w == 0) ? w_width : boundary + w_width;
    if (ws_width > 0) {
      current_w += boundary + ws_width;
    }
    last_word_end = w_end;
  }

  // Emit the final line if any
  if (last_word_end > line_start) {
    out.push_back(
        {input_line.substr(line_start, last_word_end - line_start), current_y});
  }

  return out;
}

// Split by pipes and wrap with flow (for text flowing around images)
std::vector<FlowLine> Font::split_and_wrap_with_flow(std::string_view text,
                                                     int start_y,
                                                     const FlowColumns& flow,
                                                     const TextStyle& style) {
  auto pipe_lines = split_by_pipe(text);
  std::vector<FlowLine> final_lines;
  int current_y = start_y;
  int spacing = line_spacing() + style.line_spacing;

  // For each user-specified line (split by `|`), do word wrapping
  for (std::string_view line : pipe_lines) {
    auto wrapped = word_wrap_with_flow(line, current_y, flow, style);

    if (!wrapped.empty()) {
      // Update current_y to be after the last line of this paragraph
      current_y = wrapped.back().baseline_y + spacing;
      final_lines.insert(final_lines.end(),
                         std::make_move_iterator(wrapped.begin()),
                         std::make_move_iterator(wrapped.end()));
    }
  }

  return final_lines;
}

int Font::line_spacing() const {
  // These are the numbers from the origingal, but times two because of our
  // double scaling
  switch (face_) {
  case FontFace::ASTANDAR:
    return 9 * 2;
  case FontFace::CSTANDAR:
  case FontFace::SUPERSMA:
    return 6 * 2;
  case FontFace::MINI:
    return 3 * 2; // half of supersma
  case FontFace::MENU:
    return 15 * 2;
  case FontFace::PIZ_NUMB:
    return 9 * 2;
  case FontFace::RUBY:
    return 14 * 2;
    break;
  default:
    fatal("unknown font, don't know line size");
  }
}

int Font::get_character_spacing(const TextStyle& style) const {
  // If explicitly set in style, use that value
  if (style.character_spacing.has_value()) {
    return *style.character_spacing;
  }

  // MINI, SUPERSMA, and CSTANDAR have no inter-character spacing
  if (face_ == FontFace::MINI || face_ == FontFace::SUPERSMA ||
      face_ == FontFace::CSTANDAR) {
    return 0;
  }

  // Default spacing for other fonts
  return SPACE_BETWEEN_CHARACTERS;
}

RenderedTextMetrics Font::render_fixed_width(Surface& surface,
                                             std::string_view text,
                                             ViewPoint pos, int max_width,
                                             const TextStyle& style) {
  RenderedTextMetrics metrics;
  auto lines = split_and_wrap_text(text, max_width, style);

  int total_height = 0;
  int current_y = pos.y;
  int initial_x = -1;

  for (const auto& line : lines) {
    auto line_metrics =
        render_line_onto(surface, line, ViewPoint{pos.x, current_y}, style);

    int vertical_line_spacing = line_spacing() + style.line_spacing;
    current_y += vertical_line_spacing;
    total_height += vertical_line_spacing;

    metrics.width = std::max(line_metrics.width, metrics.width);
    if (initial_x == -1) {
      initial_x = line_metrics.render_x;
    }
  }

  if (style.shadow && style.shadow->offset.dy > 0) {
    total_height += style.shadow->offset.dy;
  }

  metrics.height = total_height;
  metrics.render_x = initial_x;
  return metrics;
}

RenderedTextMetrics Font::render_into_rect(Surface& surface,
                                           std::string_view text, ViewRect rect,
                                           const TextStyle& style) {
  int max_width = rect.w;
  if (style.shadow && style.shadow->offset.dx > 0) {
    max_width -= style.shadow->offset.dx;
  }
  return render_fixed_width(surface, text, {rect.x, rect.y}, max_width, style);
}
