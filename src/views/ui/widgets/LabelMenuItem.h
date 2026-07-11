/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include "engine/geom/Spaces.h"
#include "resources/FontManager.h"
#include "views/ui/widgets/MenuItem.h"

class LabelMenuItem : public MenuItem {
public:
  LabelMenuItem(GameContext& gc, std::string text, FontFace font_face,
                const TextStyle& ts = {})
      : MenuItem(gc, font_face, std::move(text), ts.align), style_(ts) {}

  void draw(Surface& screen, ViewPoint p, bool /*is_selected*/) override {
    TextStyle style = style_;
    // Menu already aligns the layout, so we have to use Left here regardless:
    style.align = AlignX::Left;
    RenderedTextMetrics metrics =
        context_.font->render_text_onto(screen, text_, p, font_face_, style);
    width_ = metrics.width;
    height_ = metrics.height;
    x_pos_ = metrics.render_x;
  }
  void handle_action() override {}
  Size measure() const override {
    RenderedTextMetrics metrics = context_.font->size_text(text_, font_face_);
    return {metrics.width, metrics.height};
  }
  bool focusable() const override { return false; }
  bool interactive() const override { return false; }

private:
  const TextStyle style_;
};
