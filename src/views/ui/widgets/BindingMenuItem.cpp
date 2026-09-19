/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/BindingMenuItem.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "engine/geom/Spaces.h"
#include "engine/input/BindingsFile.h"
#include "engine/input/InputManager.h"
#include "engine/ui/TextAnchor.h"
#include "resources/FontManager.h"

namespace {
constexpr uint8_t color_selected = 29;
// Leaves room for a key chord and a gamepad stick direction.
constexpr int value_x = 340;
} // namespace

BindingMenuItem::BindingMenuItem(GameContext& context, std::string label,
                                 input::Action action, RowCallback on_choose)
    : MenuItem(context, FontFace::PIZ_NUMB, std::move(label)), action_(action),
      on_choose_(std::move(on_choose)) {}

std::string BindingMenuItem::value_text() const {
  if (waiting_) {
    return "Press a key...";
  }
  const std::vector<std::string> tokens =
      input::binding_tokens(*context_.input, action_);
  if (tokens.empty()) {
    return "-";
  }
  std::string joined;
  for (const std::string& token : tokens) {
    if (!joined.empty()) {
      joined += ", ";
    }
    joined += token;
  }
  return joined;
}

void BindingMenuItem::draw(Surface& screen, ViewPoint p, bool is_selected) {
  const uint8_t color = is_selected || waiting_ ? color_selected : idle_color();
  RenderedTextMetrics metrics = context_.font->render_text_onto(
      screen, text_, p, font_face_, color, TextAnchor::Start);
  height_ = metrics.height;
  metrics = context_.font->render_text_onto(screen, value_text(),
                                            ViewPoint{value_x, p.y}, font_face_,
                                            color, TextAnchor::Start);
  width_ = value_x + metrics.width;
}

Size BindingMenuItem::measure() const {
  const RenderedTextMetrics metrics =
      context_.font->size_text(value_text(), font_face_);
  return Size{value_x + metrics.width, metrics.height};
}
