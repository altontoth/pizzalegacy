/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "Font.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utfcpp/utf8.h>
#include <vector>
#include "engine/Fatal.h"
#include "engine/StringUtil.h"
#include "engine/ui/TextAnchor.h"
#include "resources/FontTypes.h"

namespace {

// Decodes the codepoint starting at `pos` and advances it. Malformed bytes
// yield U+FFFD so that arbitrary user input can still be measured and drawn.
uint32_t next_codepoint(std::string_view text, std::size_t& pos) {
  const char* it = text.data() + pos;
  const char* end = text.data() + text.size();
  utf8::utfchar32_t cp = 0;
  if (utf8::internal::validate_next(it, end, cp) != utf8::internal::UTF8_OK) {
    ++pos;
    return 0xFFFD;
  }
  pos = static_cast<std::size_t>(it - text.data());
  return static_cast<uint32_t>(cp);
}

// One whitespace-delimited word and the whitespace that follows it.
struct ScannedWord {
  std::string_view word;
  std::string_view trailing_ws;
};

bool is_space(char c) {
  return std::isspace(static_cast<unsigned char>(c)) != 0;
}

void skip_whitespace(std::string_view line, std::size_t& i) {
  while (i < line.size() && is_space(line[i])) {
    ++i;
  }
}

// Reads the word at index `i` and leaves `i` on the next word, or at the end.
ScannedWord next_word(std::string_view line, std::size_t& i) {
  const std::size_t word_begin = i;
  while (i < line.size() && !is_space(line[i])) {
    ++i;
  }
  const std::size_t word_end = i;
  skip_whitespace(line, i);
  return {line.substr(word_begin, word_end - word_begin),
          line.substr(word_end, i - word_end)};
}

// Wrapping measures the text itself.
TextStyle without_shadow(const TextStyle& style) {
  TextStyle stripped = style;
  stripped.shadow = std::nullopt;
  return stripped;
}

// Rejoins pipe-split segments into one line, with a single space where each
// '|' was.
std::string join_pipe_lines(std::span<const std::string_view> lines) {
  std::string joined;
  for (std::string_view line : lines) {
    line = util::trim(line);
    if (line.empty()) {
      continue;
    }
    if (!joined.empty()) {
      joined += ' ';
    }
    joined += line;
  }
  return joined;
}

} // namespace

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

  // Flow text path: variable width wrapping, which never breaks at a soft
  // hyphen.
  if (flow.has_value()) {
    const std::string cleaned_text = style.soft_hyphens
                                         ? analyze_word_breaks(text).clean_text
                                         : std::string(text);

    // Get lines with their Y positions
    auto flow_lines =
        split_and_wrap_with_flow(cleaned_text, pos.y, *flow, style);

    int total_height = 0;
    int max_width = 0;
    int initial_x = -1;

    // Render each line at its designated position
    for (const auto& flow_line : flow_lines) {
      // Determine X position based on Y position
      int x = (flow_line.baseline_y < flow->cutoff_y) ? flow->left_margin_above
                                                      : flow->left_margin_below;

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

  // No flow, so we just wrap at the edge of the surface (after taking the
  // anchor into account).
  int available_width = surface.width() - pos.x;
  switch (style.align) {
  case TextAnchor::Start:
    break;
  case TextAnchor::Middle:
    available_width = 2 * std::min(pos.x, surface.width() - pos.x);
    break;
  case TextAnchor::End:
    available_width = pos.x;
    break;
  }
  if (style.shadow && style.shadow->offset.dx > 0) {
    available_width -= style.shadow->offset.dx;
  }
  return render_fixed_width(surface, text, pos, available_width, style);
}

RenderedTextMetrics Font::size_text(std::string_view text,
                                    const TextStyle& style) {
  RenderedTextMetrics metrics;

  std::size_t pos = 0;
  while (pos < text.size()) {
    const Glyph* glyph = find_glyph(next_codepoint(text, pos));
    if (glyph != nullptr) {
      // We need one fewer space between characters than number of
      // characters in our string (3 chars == 2 spaces, etc):
      if (pos < text.size()) {
        metrics.width += get_character_spacing(style);
      }
      metrics.width += advance_width(*glyph);
      metrics.height = std::max(metrics.height, (int)glyph->height);
    }
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

RenderedTextMetrics Font::size_text(std::string_view text, int max_width,
                                    const TextStyle& style) {
  auto lines = split_and_wrap_text(text, max_width, style);
  RenderedTextMetrics metrics;
  for (const auto& line : lines) {
    auto line_metrics = size_text(line, style);
    metrics.width = std::max(metrics.width, line_metrics.width);
    metrics.height += line_spacing() + style.line_spacing;
  }
  if (style.shadow && style.shadow->offset.dy > 0) {
    metrics.height += style.shadow->offset.dy;
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

  ViewPoint origin{pos.x, pos.y};
  switch (style.align) {
  case TextAnchor::Start:
    break;
  case TextAnchor::Middle:
    origin.x = pos.x - (size.width / 2);
    break;
  case TextAnchor::End:
    origin.x = pos.x - size.width;
    break;
  }
  out.render_x = origin.x;

  // We'll render twice if shadow is enabled; first shadow then foreground
  auto render_pass = [&](ViewPoint pos, uint8_t color) {
    ViewPoint cur = pos;

    std::size_t offset = 0;
    while (offset < text.size()) {
      const Glyph* glyph = find_glyph(next_codepoint(text, offset));
      if (glyph != nullptr) {
        const Glyph& g = *glyph;

        if (g.width > 0 && color != gfx::TRANSPARENT) {
          const ViewRect src{g.start_position_x, g.start_position_y, g.width,
                             g.height};
          target.blit_stencil(data_.surface, src, {cur.x, pos.y}, color);
        }

        cur.x += advance_width(g);

        // Only add spacing if NOT the last character
        if (offset < text.size()) {
          cur.x += get_character_spacing(style);
        }
      }
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

uint32_t Font::remap_character(uint32_t codepoint) const {
  if (face_ == FontFace::SUPERSMA || face_ == FontFace::MINI) {
    if (codepoint == '\\') {
      return '/';
    }
  }
  return codepoint;
}

uint32_t Font::substitute_character(uint32_t codepoint) const {
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
  skip_whitespace(input_line, i);

  const int boundary = get_character_spacing(style);
  const TextStyle wrap_style = without_shadow(style);

  std::string current_line;
  int current_w = 0;
  // Trailing whitespace of the previous word, inserted before the next word so
  // runs of spaces survive for manual column alignment with spaces. But don't
  // do that :) If we encounter a line break we drop the rest of the whitespace.
  std::string pending_ws;

  while (i < n) {
    const auto [raw_word, ws] = next_word(input_line, i);
    BreakableWord bw;
    if (style.soft_hyphens) {
      bw = analyze_word_breaks(raw_word);
    }
    else {
      bw.clean_text = std::string(raw_word);
    }
    const int w_width = size_text(bw.clean_text, wrap_style).width;
    const int ws_width = ws.empty() ? 0 : size_text(ws, wrap_style).width;

    // Does the clean word fit on the current line?
    int needed = (current_w == 0) ? w_width : current_w + boundary + w_width;
    if (needed <= max_width) {
      // Fits whole
      if (!current_line.empty()) {
        current_line += pending_ws;
      }
      current_line += bw.clean_text;
      current_w = needed;
      pending_ws.assign(ws);
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
      int best_break = -1;

      for (auto pos : bw.break_positions) {
        const std::string fragment = bw.clean_text.substr(0, pos) + '-';
        const int frag_w = size_text(fragment, wrap_style).width;
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
          current_line += pending_ws;
        }
        current_line += bw.clean_text.substr(0, best_break) + '-';
        out.push_back(std::move(current_line));
        current_line.clear();

        // remainder becomes the start of the next line
        auto remainder = bw.clean_text.substr(best_break);
        current_w = size_text(remainder, wrap_style).width;
        current_line = std::move(remainder);
        pending_ws.assign(ws);
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
    pending_ws.assign(ws);
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
  const auto pipe_lines = split_by_pipe(text);

  std::vector<std::string> wrapped_pipe_lines;
  bool pipe_break_overflowed = false;
  for (std::string_view line : pipe_lines) {
    auto wrapped = word_wrap_line(line, max_width, style);
    if (wrapped.size() > 1) {
      pipe_break_overflowed = true;
    }
    for (auto& w : wrapped) {
      wrapped_pipe_lines.push_back(std::move(w));
    }
  }

  // Keep the '|' breaks only while the wrapping it produces fits. If not,
  // remove them and do the rewrapping ourselves.
  if (style.reflow_pipe_lines && pipe_lines.size() > 1 &&
      pipe_break_overflowed) {
    return word_wrap_line(join_pipe_lines(pipe_lines), max_width, style);
  }
  return wrapped_pipe_lines;
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
  skip_whitespace(input_line, i);

  const int boundary = get_character_spacing(style);
  const TextStyle wrap_style = without_shadow(style);

  std::size_t line_start = i;
  std::size_t last_word_end = i;
  int current_w = 0;
  int current_y = start_y;
  int vert_spacing = line_spacing() + style.line_spacing;

  while (i < n) {
    // Determine current max width based on Y position
    int left_margin = (current_y < flow.cutoff_y) ? flow.left_margin_above
                                                  : flow.left_margin_below;
    int max_width = flow.right_margin - left_margin;

    const std::size_t word_begin = i;
    const auto [word_view, ws_view] = next_word(input_line, i);
    const std::size_t word_end = word_begin + word_view.size();
    const int w_width = size_text(word_view, wrap_style).width;
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
      line_start = word_begin;
      current_w = 0;
    }

    // Accumulate: word (with boundary spacing if joining to existing content)
    current_w += (current_w == 0) ? w_width : boundary + w_width;
    if (ws_width > 0) {
      current_w += boundary + ws_width;
    }
    last_word_end = word_end;
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

const Glyph* Font::find_glyph(uint32_t codepoint) const {
  const uint32_t cp = remap_character(codepoint);
  auto it = data_.glyphs.find(cp);
  if (it == data_.glyphs.end()) {
    it = data_.glyphs.find(substitute_character(cp));
  }
  return it == data_.glyphs.end() ? nullptr : &it->second;
}

int Font::advance_width(const Glyph& glyph) const { return glyph.advance; }

int Font::get_character_spacing(const TextStyle& style) const {
  return style.character_spacing.value_or(0);
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
