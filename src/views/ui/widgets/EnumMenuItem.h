/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>

#include "engine/geom/Spaces.h"
#include "resources/FontManager.h"
#include "views/ui/widgets/MenuItem.h"

template <typename E> class EnumMenuItem : public MenuItem {
public:
  EnumMenuItem(GameContext& context, const std::string& text, E& value,
               std::vector<E> options, std::function<void()> action,
               FontFace font_face = FontFace::PIZ_NUMB)
      : MenuItem(context, font_face, text), context_(context), value_(value),
        options_(std::move(options)), action_(std::move(action)) {}

  void draw(Surface& screen, ViewPoint p, bool is_selected) override {
    constexpr int value_x_pos = 400;
    uint8_t color = is_selected ? 29 : idle_color();

    RenderedTextMetrics metrics = context_.font->render_text_onto(
        screen, text_, p, font_face_, color, TextAnchor::Start);

    width_ = metrics.width;
    height_ = metrics.height;

    std::string label{to_string(value_)};
    context_.font->render_text_onto(screen, label, ViewPoint{value_x_pos, p.y},
                                    font_face_, color, TextAnchor::Start);
    width_ += (value_x_pos - width_);
  }

  void handle_action() override {
    cycle(+1);
    if (action_) {
      action_();
    }
  }

  bool on_adjust(AdjustAxis axis, int delta) override {
    if (axis != AdjustAxis::Horizontal) {
      return false;
    }
    cycle(delta);
    if (action_) {
      action_();
    }
    return true;
  }

  Size measure() const override {
    RenderedTextMetrics metrics = context_.font->size_text(text_, font_face_);
    std::string label{to_string(value_)};
    RenderedTextMetrics val_metrics =
        context_.font->size_text(label, font_face_);
    constexpr int value_x_pos = 400;
    return Size{value_x_pos + val_metrics.width, metrics.height};
  }

  // Step to the neighboring option; delta is -1 or +1, wrapping around.
  void cycle(int delta) {
    for (size_t i = 0; i < options_.size(); ++i) {
      if (options_[i] == value_) {
        value_ = options_[(i + options_.size() + delta) % options_.size()];
        return;
      }
    }
    if (!options_.empty()) {
      value_ = options_[0];
    }
  }

private:
  GameContext& context_;
  E& value_;
  std::vector<E> options_;
  std::function<void()> action_;
};
