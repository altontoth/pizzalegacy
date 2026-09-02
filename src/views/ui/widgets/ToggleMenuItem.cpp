/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/ToggleMenuItem.h"
#include "engine/geom/Spaces.h"
#include "resources/FontManager.h"

constexpr int COLOR_HOVER = 29;   // Green
constexpr int COLOR_NORMAL = 240; // Black
constexpr int toggle_behind_label_offset = 400;

// Original constructor - delegates to new constructor with default labels
ToggleMenuItem::ToggleMenuItem(GameContext& context, const std::string& text,
                               bool& value, std::function<void()> action,
                               FontFace font_face)
    : ToggleMenuItem(context, text, value, "OFF", "ON", action, font_face) {}

// New constructor with custom labels
ToggleMenuItem::ToggleMenuItem(GameContext& context, const std::string& text,
                               bool& value, const std::string& false_label,
                               const std::string& true_label,
                               std::function<void()> action, FontFace font_face)
    : MenuItem(context, font_face, text), value_(value), action_(action),
      false_label_(false_label), true_label_(true_label) {}

void ToggleMenuItem::draw(Surface& screen, ViewPoint p, bool is_selected) {
  const std::string& value_text = value_ ? true_label_ : false_label_;

  RenderedTextMetrics metrics = context_.font->render_text_onto(
      screen, text_, p, font_face_, is_selected ? COLOR_HOVER : COLOR_NORMAL,
      TextAnchor::Start);

  width_ = metrics.width;
  height_ = metrics.height;

  metrics = context_.font->render_text_onto(
      screen, value_text, ViewPoint{toggle_behind_label_offset, p.y},
      font_face_, is_selected ? COLOR_HOVER : COLOR_NORMAL, TextAnchor::Start);
  width_ += (toggle_behind_label_offset - width_);
}

Size ToggleMenuItem::measure() const {
  const std::string& value_text = value_ ? true_label_ : false_label_;

  RenderedTextMetrics metrics = context_.font->size_text(text_, font_face_);

  metrics = context_.font->size_text(value_text, font_face_);
  metrics.width += toggle_behind_label_offset;
  return Size{metrics.width, metrics.height};
}

void ToggleMenuItem::handle_action() { action_(); }
