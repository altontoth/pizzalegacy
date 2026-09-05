/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/CheckboxMenuItem.h"

#include <algorithm>

#include "engine/gfx/Draw.h"
#include "engine/ui/TextAnchor.h"
#include "resources/FontManager.h"

constexpr int box_size = 14;
constexpr int box_thickness = 1;
// Clearance between the frame and the cross.
constexpr int cross_padding = 1;
constexpr int label_gap = 6;
constexpr uint8_t color_normal = 1;
constexpr uint8_t color_selected = 29;

CheckboxMenuItem::CheckboxMenuItem(GameContext& gc, const std::string& text,
                                   bool& value, std::function<void()> action,
                                   FontFace font_face)
    : MenuItem(gc, font_face, text), value_(value), action_(std::move(action)) {
}

void CheckboxMenuItem::draw(Surface& screen, ViewPoint p, bool is_selected) {
  const uint8_t color = is_selected ? color_selected : color_normal;

  // Snap to an even coordinate so line_2x draws a square cross inside the box.
  ViewPoint box{p.x, p.y};
  if (box.x % 2 != 0) {
    box.x += 1;
  }
  if (box.y % 2 != 0) {
    box.y += 1;
  }

  gfx::draw::box(screen, ViewRect{box.x, box.y, box_size, box_size}, color,
                 box_thickness);

  if (value_) {
    const int start = box_thickness + cross_padding;
    const int end = box_size - start - 2;
    // Draw the cross.
    gfx::draw::line_2x(screen, ViewPoint{box.x + start, box.y + start},
                       ViewPoint{box.x + end, box.y + end}, color);
    gfx::draw::line_2x(screen, ViewPoint{box.x + end, box.y + start},
                       ViewPoint{box.x + start, box.y + end}, color);
  }

  RenderedTextMetrics metrics = context_.font->render_text_onto(
      screen, text_, ViewPoint{box.x + box_size + label_gap, p.y}, font_face_,
      color, TextAnchor::Start);

  width_ = box_size + label_gap + metrics.width;
  height_ = std::max(box_size, metrics.height);
}

Size CheckboxMenuItem::measure() const {
  RenderedTextMetrics metrics = context_.font->size_text(text_, font_face_);
  return Size{box_size + label_gap + metrics.width,
              std::max(box_size, metrics.height)};
}

bool CheckboxMenuItem::on_adjust(AdjustAxis axis, int /*delta*/) {
  if (axis != AdjustAxis::Horizontal) {
    return false;
  }
  handle_action();
  return true;
}

void CheckboxMenuItem::handle_action() { action_(); }
