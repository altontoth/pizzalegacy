/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/TextMenuItem.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/TextAnchor.h"
#include "resources/FontManager.h"

void TextMenuItem::draw(Surface& screen, ViewPoint p, bool is_selected) {
  TextStyle style = is_selected ? selected_style_ : style_;
  style.align = TextAnchor::Start;
  RenderedTextMetrics metrics =
      context_.font->render_text_onto(screen, text_, p, font_face_, style);

  width_ = metrics.width;
  height_ = metrics.height;
  x_pos_ = metrics.render_x;
}

Size TextMenuItem::measure() const {
  RenderedTextMetrics metrics = context_.font->size_text(text_, font_face_);
  return {metrics.width, metrics.height};
}

void TextMenuItem::handle_action() { action_(); }
